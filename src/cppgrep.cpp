#include <algorithm>
#include <filesystem>
#include <fstream>
#include <variant>

#include "cppgrep.h"

#include "util/log.h"
#include "util/sys.h"

namespace fs = std::filesystem;
using namespace util;

using util::misc::ThreadPool;

namespace cppgrep {

/// Implementation namespace of the grep functionality.
/// Not part of the public interface.
namespace impl {

/// Represents a small string or string_view that delimits the pattern.
using affix = std::variant<std::string, std::string_view>;

/// Checks for tabs and newlines in a string_view. Returns a new string if a replace was necessary.
affix replace_tab_and_newline(std::string_view affix) noexcept;

/// Finds the proper offset skip range for the search iterator.
/// Probably not necessary when used with std::boyer_moore_searcher.
constexpr size_t overlap_offset(std::string_view pattern) noexcept;

/// Checks if the input args are valid.
opt_err validate_args(std::string_view path, std::string_view pattern) noexcept;

/// Checks if a path is an accessible file or directory.
opt_err validate_path(const fs::path& path) noexcept;

/// Searches a text pattern in a file.
void grep_file(const fs::path& file_path, std::string_view pattern, ThreadPool::Ptr thread_pool = nullptr);

/// Recursively iterates a directory and searches a text pattern in each valid file.
void grep_dir(const fs::path& dir_path, std::string_view pattern, ThreadPool::Ptr thread_pool = nullptr);

constexpr size_t overlap_offset(std::string_view pattern) noexcept
{
    auto pos {0};

    auto subpattern = pattern.substr(0, pattern.size() - 1);
    while (subpattern.size() > 1)
    {
        subpattern = pattern.substr(0, subpattern.size() - 1);
        pos        = pattern.find_last_of(subpattern);
    }

    return pos > 0 ? pos : pattern.size();
}

affix replace_tab_and_newline(std::string_view affix) noexcept
{
    auto find = std::find_if(affix.begin(), affix.end(), [](char c) {
        return c == '\t' || c == '\n' || c == '\r';
    });

    if (find != affix.end())
    {
        std::string out {affix.begin(), find};
        for (auto it = ++find; it != affix.end(); ++it)
        {
            switch (*it)
            {
                case '\t':
                    out += "\\t";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                default:
                    out += *it;
            }
        }
        return out;
    }

    return affix;
}

opt_err validate_args(std::string_view path, std::string_view pattern) noexcept
{
    if (pattern.size() > MAX_PATTERN_SIZE)
    {
        return {"Pattern size exceeds the limit."};
    }

    if (auto path_check = validate_path(path); !path_check)
    {
        return path_check;
    }

    return true;
}

opt_err validate_path(const std::filesystem::path& path) noexcept
{
    try
    {
        if (!fs::exists(path))
        {
            return {"Path does not exist."};
        }

        if (!fs::is_regular_file(path) && !fs::is_directory(path))
        {
            return {"Path is not regular file or directory."};
        }

#ifdef WIN32_BUILD
        // NOTE: On Unix, fs::exists() throws if permission is denied, but not on Windows.
        // There seems to be no clean alternative, other than calling WinAPI.
        if (!sys::win32_can_read(path.u8string().c_str()))
        {
            return {"Permission denied when accessing path."};
        }
#endif
    }
    catch (fs::filesystem_error& e)
    {
        return {fmt::format_str("Unable to access path. Reason: %s", e.what())};
    }

    return true;
}

void grep_file(const std::filesystem::path& file_path, std::string_view pattern, ThreadPool::Ptr thread_pool)
{
    // TODO:
    //  - avoid a thread pool when grepping a single file which fits into a single chunk
    //  - move the lambda's logic, the statics it uses and the state it captures into a class
    //  - limit the amount of queued chunks; consider blocking thread in add_task if queue is full

    // skip file if too small
    if (fs::file_size(file_path) < pattern.size())
    {
        return;
    }

    static const auto searcher   = std::boyer_moore_searcher(pattern.begin(), pattern.end());
    static const auto chunk_size = std::max(static_cast<decltype(sys::pagesize())>(pattern.size()), sys::pagesize());
    static const auto increment  = overlap_offset(pattern);

    if (std::ifstream stream {file_path, std::ios::binary}; stream.good())
    {
        for (auto chunk_count {0UL}; true; ++chunk_count)
        {
            auto chunk = std::vector<char>(chunk_size);
            stream.read(&chunk[0], chunk_size);

            // reached eof
            if (stream.gcount() < pattern.size())
            {
                return;
            }

            // final chunk may not be a full read so don't rely on chunk.end() but rather on bytes last read
            auto task = [=, chunk {std::move(chunk)}, offset {stream.gcount()}] {
                for (auto chunk_pos = chunk.begin(), read_end = chunk.begin() + offset;
                     chunk_pos = std::search(chunk_pos, read_end, searcher), chunk_pos != read_end;
                     chunk_pos += increment)
                {
                    auto match_pos = (chunk_count * chunk_size) + (chunk_pos - chunk.begin());

                    // get affixes; some over-engineering: use string_view if no tabs or newlines, otherwise get modified string
                    // corner-case: doesn't work with affixes in-between chunks
                    auto bound     = static_cast<size_t>(std::distance(chunk.begin(), chunk_pos));
                    auto safe_dist = std::min(bound, MAX_AFFIX_SIZE);

                    const auto& get_prefix = bound ? replace_tab_and_newline({&chunk[(bound - safe_dist)], safe_dist}) : affix {};

                    bound += pattern.size();
                    safe_dist = std::min(static_cast<size_t>(offset - bound), MAX_AFFIX_SIZE);

                    const auto& get_suffix = bound < offset ? replace_tab_and_newline({&chunk[bound], MAX_AFFIX_SIZE}) : affix {};

                    auto& prefix = std::holds_alternative<std::string_view>(get_prefix) ? std::get<std::string_view>(get_prefix)
                                                                                        : std::get<std::string>(get_prefix);

                    auto& suffix = std::holds_alternative<std::string_view>(get_suffix) ? std::get<std::string_view>(get_suffix)
                                                                                        : std::get<std::string>(get_suffix);

                    // colored format is not displayed correctly in all terminals; tested OK with WSL and Windows terminals
                    std::string output = fmt::format_str("%s(%d): %.*s", file_path.u8string().c_str(), match_pos, prefix.size(), prefix)
                                         + fmt::format_str("\033[1;32m%s\033[0m", pattern)
                                         + fmt::format_str("%.*s", suffix.size(), suffix);
                    log::info(output.c_str());
                }
            };

            thread_pool ? thread_pool->add_task(task) : task();

            // overlap chunks, in case there's a match in-between
            stream.seekg(1 - static_cast<std::streamoff>(pattern.size() + MAX_AFFIX_SIZE), std::ios_base::cur);
        }
    }
}

void grep_dir(const std::filesystem::path& dir_path, std::string_view pattern, ThreadPool::Ptr thread_pool)
{
    // NOTE: After the user-provided directory path is validated for read access,
    // there is no requirement to report/handle denied access on contained entries.
    // The non accessible entries will be skipped, without informing the user.

    std::error_code ec;
    for (fs::recursive_directory_iterator it {dir_path, ec}, end; it != end; it.increment(ec))
    {
        if (ec)
        {
            it.pop();
            continue;
        }

        // fstream will invalidate bad files after this point
        if (fs::is_regular_file(it->path()))
        {
            grep_file(it->path(), pattern, thread_pool);
        }
    }
}

} // namespace impl

opt_err grep(std::string_view path, std::string_view pattern, ThreadPool::Ptr thread_pool) noexcept
{
    if (auto args_check = impl::validate_args(path, pattern); !args_check)
    {
        return args_check;
    }

    if (fs::is_regular_file(path))
    {
        log::info("The path is a regular file. Searching...");
        impl::grep_file(path, pattern, thread_pool);
    }
    else
    {
        log::info("The path is a directory. Searching recursively...");
        impl::grep_dir(path, pattern, thread_pool);
    }

    return true;
}

} // namespace cppgrep
