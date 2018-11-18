#include <algorithm>
#include <filesystem>
#include <fstream>

#include "cppgrep.h"

#include "util/log.h"
#include "util/sys.h"

namespace fs = std::filesystem;
using namespace util;

namespace cppgrep {

/// Implementation namespace of the grep functionality.
/// Not part of the public interface.
namespace impl {

/// Finds the proper offset skip range for the search iterator.
/// Probably not necessary when used with std::boyer_moore_searcher.
constexpr size_t overlap_offset(std::string_view pattern) noexcept;

/// Checks if the input args are valid.
opt_err validate_args(std::string_view path, std::string_view pattern) noexcept;

/// Checks if a text pattern meets the requirements restrictions.
opt_err validate_pattern(std::string_view pattern) noexcept;

/// Checks if a path is an accessible file or directory.
opt_err validate_path(const fs::path& path) noexcept;

/// Searches a text pattern in a file.
void grep_file(const fs::path& file_path, std::string_view pattern, util::misc::ThreadPool::Ptr thread_pool = nullptr);

/// Recursively iterates a directory and searches a text pattern in each valid file.
void grep_dir(const fs::path& dir_path, std::string_view pattern, util::misc::ThreadPool::Ptr thread_pool = nullptr);

} // namespace impl

opt_err grep(std::string_view path, std::string_view pattern, util::misc::ThreadPool::Ptr thread_pool) noexcept
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

constexpr size_t impl::overlap_offset(std::string_view pattern) noexcept
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

opt_err impl::validate_args(std::string_view path, std::string_view pattern) noexcept
{
    if (auto pattern_check = impl::validate_pattern(pattern); !pattern_check)
    {
        return pattern_check;
    }

    if (auto path_check = impl::validate_path(path); !path_check)
    {
        return path_check;
    }

    return true;
}

opt_err impl::validate_pattern(std::string_view pattern) noexcept
{
    if (pattern.size() > MAX_PATTERN_SIZE)
    {
        return {"Pattern size exceeds the limit."};
    }

    return true;
}

opt_err impl::validate_path(const std::filesystem::path& path) noexcept
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
        // NOTE: fs::exists() does not throw if permission is denied on Windows.
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

void impl::grep_file(const std::filesystem::path& file_path, std::string_view pattern, std::shared_ptr<misc::ThreadPool> thread_pool)
{
    // TODO:
    //  - avoid a thread pool when grepping a single file which fits into a single chunk
    //  - restrict search within chunk limits for the prefix/suffix requirement
    //  - move the lambda's logic and the state it captures into a class, and process objects instead
    //  - limit the total amount of memory to queue as chunks; block the read thread if buffer is full

    static const auto searcher   = std::boyer_moore_searcher(pattern.begin(), pattern.end());
    static const auto chunk_size = std::max(static_cast<decltype(sys::pagesize())>(pattern.size()), sys::pagesize());
    static const auto increment  = overlap_offset(pattern);

    if (std::ifstream stream {file_path}; stream.good())
    {
        for (auto chunk_count {0UL}; true; ++chunk_count)
        {
            auto chunk = std::vector<char>(chunk_size);
            stream.read(&chunk[0], chunk_size);

            // not enough bytes read; small file or eof
            if (stream.gcount() < pattern.size())
            {
                break;
            }

            auto task = [=, chunk {std::move(chunk)}, offset {stream.gcount()}] {
                for (auto chunk_pos = chunk.begin(), chunk_end = chunk.begin() + offset;
                     chunk_pos = std::search(chunk_pos, chunk_end, searcher), chunk_pos != chunk_end;
                     chunk_pos += increment)
                {
                    auto match_pos = (chunk_count * chunk_size) + (chunk_pos - chunk.begin());
                    log::info("Found match at: %s, %d", file_path.u8string().c_str(), match_pos);
                }
            };

            if (thread_pool)
            {
                thread_pool->add_task(task);
            }
            else
            {
                task();
            }

            // overlap chunks, in case there's a match in between
            stream.seekg(1 - static_cast<std::streamoff>(pattern.size()), std::ios_base::cur);
        }
    }
}

void impl::grep_dir(const std::filesystem::path& dir_path, std::string_view pattern, std::shared_ptr<misc::ThreadPool> thread_pool)
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

} // namespace cppgrep
