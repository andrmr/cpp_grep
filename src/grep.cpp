#include <algorithm>
#include <filesystem>
#include <fstream>
#include <variant>

#include "grep.h"

#include "util/log.h"
#include "util/optional_error_bool.h"
#include "util/sys.h"

namespace fs = std::filesystem;
using namespace util;

using opt_err = util::misc::OptionalErrorBool; //!< Return type that acts as a bool but can hold an error message.

namespace cppgrep {

/// Implementation of helper functions not needed in the public interface.
namespace impl {

/// Represents a string or string_view that delimits the pattern.
using affix = std::variant<std::string, std::string_view>;

/// Checks for tabs and newlines in a string_view. Returns a new string if a replace was necessary.
affix replace_tab_and_newline(std::string_view affix) noexcept;

/// Finds the proper offset skip range for the search iterator.
/// eg. when searching and finding the word "test" skip to last "t" in case another match starts there
/// Maybe not necessary when used with std::boyer_moore_searcher.
constexpr size_t overlap_offset(std::string_view pattern) noexcept;

/// Checks if the input args are valid.
opt_err validate_args(std::string_view path, std::string_view pattern) noexcept;

/// Checks if a path is an accessible file or directory.
opt_err validate_path(const fs::path& path) noexcept;

} // namespace impl

Grep Grep::build_grep(std::string_view path, std::string_view pattern, unsigned int threads)
{
    if (auto args_check = impl::validate_args(path, pattern); !args_check)
    {
        throw std::invalid_argument {args_check.error().value_or("Unknown error occured when validating arguments.")};
    }

    return Grep(path, pattern, threads);
}

Grep::Grep(std::string_view path, std::string_view pattern, unsigned int threads)
    : m_path {path},
      m_pattern {pattern},
      m_searcher {pattern.begin(), pattern.end()},
      m_chunk_size {std::max<size_t>(sys::pagesize(), pattern.size())},
      m_increment {impl::overlap_offset(pattern)},
      m_threadpool {threads ? std::make_unique<util::misc::ThreadPool>(threads) : nullptr}
{
}

unsigned long Grep::search() noexcept
{
    if (fs::is_regular_file(m_path))
    {
        log::info("The path is a regular file. Searching...");
        grep_file(m_path, true);
    }
    else
    {
        log::info("The path is a directory. Searching recursively...");
        grep_dir(m_path);
    }

    return m_result_count;
}

void Grep::grep_file(const std::filesystem::path& file_path, bool single_file)
{
    // TODO:
    //  - don't start all the thread pool threads at once
    //  - limit the amount of queued chunks; consider blocking thread in add_task if queue is full

    // skip file if logical size is too small
    if (fs::file_size(file_path) < m_pattern.size())
    {
        return;
    }

    if (std::ifstream stream {file_path, std::ios::binary}; stream.good())
    {
        // need shared ptr for multithreaded chunks that use same filename
        // this isn't optimal for single thread or single file use case
        auto file_name = std::make_shared<std::string>(file_path.u8string());

        for (auto chunk_count {0UL}; true; ++chunk_count)
        {
            auto chunk = std::vector<char>(m_chunk_size);
            stream.read(&chunk[0], m_chunk_size);

            // reached eof
            if (stream.gcount() < m_pattern.size())
            {
                return;
            }

            // final chunk may not be a full read so don't rely on chunk.end() but rather on bytes last read
            auto offset = stream.gcount();

            // don't queue to thread pool if grepping a single small file or when not using a pool
            auto threaded = m_threadpool && !(single_file && fs::file_size(file_path) < m_chunk_size);
            if (threaded)
            {
                auto task = [&, chunk {std::move(chunk)}, offset, chunk_count, file_name] {
                    grep_chunk(chunk, offset, chunk_count, file_name);
                };

                m_threadpool->add_task(task);
            }
            else
            {
                grep_chunk(chunk, offset, chunk_count, file_name);
            }

            // overlap chunks, in case there's a match in-between
            stream.seekg(1 - static_cast<std::streamoff>(m_pattern.size() + MAX_AFFIX_SIZE), std::ios_base::cur);
        }
    }
}

void Grep::grep_dir(const std::filesystem::path& dir_path)
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

        // fstream will validate files after this point
        if (fs::is_regular_file(it->path()))
        {
            grep_file(it->path());
        }
    }
}

void Grep::grep_chunk(const std::vector<char>& chunk, unsigned long offset, unsigned long chunk_count, std::shared_ptr<std::string> file_name)
{
    for (auto chunk_pos = chunk.begin(), read_end = chunk.begin() + offset;
         chunk_pos = std::search(chunk_pos, read_end, m_searcher), chunk_pos != read_end;
         chunk_pos += m_increment)
    {
        ++m_result_count;
        auto result_pos = (chunk_count * m_chunk_size) + (chunk_pos - chunk.begin());

        // get affixes; corner-case: doesn't work with affixes in-between chunks
        auto boundary   = static_cast<size_t>(std::distance(chunk.begin(), chunk_pos));
        auto safe_dist  = std::min<size_t>(boundary, MAX_AFFIX_SIZE);
        auto get_prefix = boundary ? impl::replace_tab_and_newline({&chunk[(boundary - safe_dist)], safe_dist}) : impl::affix {};

        boundary += m_pattern.size();
        safe_dist       = std::min<int>(static_cast<size_t>(offset - boundary), MAX_AFFIX_SIZE);
        auto get_suffix = boundary < offset ? impl::replace_tab_and_newline({&chunk[boundary], MAX_AFFIX_SIZE}) : impl::affix {};

        auto prefix = std::holds_alternative<std::string_view>(get_prefix) ? std::get<std::string_view>(get_prefix)
                                                                           : std::get<std::string>(get_prefix);

        auto suffix = std::holds_alternative<std::string_view>(get_suffix) ? std::get<std::string_view>(get_suffix)
                                                                           : std::get<std::string>(get_suffix);

        // color format is not displayed correctly on all terminals
        std::string output = fmt::format_str("%s(%d): %.*s", file_name->c_str(), result_pos, prefix.size(), prefix)
                             + fmt::format_str("\033[1;32m%s\033[0m", m_pattern.c_str())
                             + fmt::format_str("%.*s", suffix.size(), suffix);
        log::info(output.c_str());
    }
}

impl::affix impl::replace_tab_and_newline(std::string_view affix) noexcept
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

constexpr size_t impl::overlap_offset(std::string_view pattern) noexcept
{
    auto position {0};

    auto subpattern = pattern.substr(0, pattern.size() - 1);
    while (subpattern.size() > 1)
    {
        subpattern = pattern.substr(0, subpattern.size() - 1);
        position   = pattern.find_last_of(subpattern);
    }

    return position > 0 ? position : pattern.size();
}

opt_err impl::validate_args(std::string_view path, std::string_view pattern) noexcept
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

opt_err impl::validate_path(const fs::path& path) noexcept
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

} // namespace cppgrep
