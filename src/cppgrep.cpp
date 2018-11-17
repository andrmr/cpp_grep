#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

#include "util/bool_result.h"
#include "util/log.h"
#include "util/sys.h"
#include "util/thread_pool.h"

namespace fs = std::filesystem;
using namespace util;
using extended_bool = util::misc::BoolResult;

namespace {
constexpr auto MAX_PATTERN_SIZE {128U}; //!< Max pattern size, in characters.
constexpr auto MAX_AFFIX_SIZE {3U};     //!< Max affix size, in characters.
} // namespace

/// Checks if a text pattern meets the requirements restrictions.
extended_bool is_valid(std::string_view pattern)
{
    if (pattern.size() > MAX_PATTERN_SIZE)
    {
        return {"Pattern size exceeds the limit."};
    }

    return true;
}

/// Checks if a path exists and can be accessed.
extended_bool is_accessible(const fs::path& path)
{
    try
    {
        if (!fs::exists(path))
        {
            return {"Path does not exist."};
        }
    }
    catch (fs::filesystem_error& e)
    {
        if (e.code() == std::errc::permission_denied)
        {
            return {"Permission denied when accessing path."};
        }
        else
        {
            return {log::string_format("Unable to access path. Exception: %s", e.what())};
        }
    }

    return true;
}

/// Checks if a path is a accessible file or directory.
extended_bool is_valid(const fs::path& path)
{
    if (auto accessible = is_accessible(path); !accessible)
    {
        return accessible;
    }

    if (!fs::is_regular_file(path) && !fs::is_directory(path))
    {
        return {"Path is not regular file or directory."};
    }

    return true;
}

/// Searches a text pattern in a file.
void grep_file(const fs::path& file_path, std::string_view pattern)
{
    // todo: avoid a thread pool when grepping a single file which fits into a single chunk
    // todo: restrict search within chunk limits for the prefix/suffix requirement
    // todo: move the lambda's logic and the state it captures into a class, and process objects instead
    // todo: limit the total amount of memory to queue as chunks; block the read thread if buffer is full

    static util::tp::ThreadPool thread_pool {};
    static const auto searcher   = std::boyer_moore_searcher(pattern.begin(), pattern.end());
    static const auto chunk_size = std::max(static_cast<decltype(sys::pagesize())>(pattern.size()), sys::pagesize());

    if (fs::file_size(file_path) < pattern.size())
    {
        return;
    }

    if (std::ifstream stream {file_path}; stream.good())
    {
        for (auto chunk_count {0UL}; true; ++chunk_count)
        {
            auto chunk = std::vector<char>(chunk_size);
            stream.read(&chunk[0], chunk_size);

            // not enough bytes left
            if (stream.gcount() < pattern.size())
            {
                break;
            }

            auto task = [=, chunk {std::move(chunk)}, offset {stream.gcount()}] {
                for (auto chunk_pos = chunk.begin(), chunk_end = chunk.begin() + offset;
                     chunk_pos = std::search(chunk_pos, chunk_end, searcher), chunk_pos != chunk_end;
                     chunk_pos += pattern.size())
                {
                    auto match_pos = (chunk_count * chunk_size) + (chunk_pos - chunk.begin());
                    log::info("Found match at: %s, %d", file_path.u8string().c_str(), match_pos);
                }
            };

            thread_pool.add_task(task);

            // overlap chunks, in case there's a match in between
            stream.seekg(1 - static_cast<std::streamoff>(pattern.size()), std::ios_base::cur);
        }
    }
}

/// Recursively iterates a directory and searches a text pattern in each valid file.
void grep_dir(const fs::path& dir_path, std::string_view pattern)
{
    // note: range-for recursive iterator throws, if permission is denied

    std::error_code ec;
    for (fs::recursive_directory_iterator it {dir_path}, end; it != end; it.increment(ec))
    {
        if (is_valid(it->path()) && fs::is_regular_file(it->path()))
        {
            grep_file(it->path(), pattern);
        }
    }
}

void grep(std::string_view path, std::string_view pattern)
{
    auto valid_p = is_valid(pattern);
    if (valid_p.error())
    {
        log::info(valid_p.error().value());
    }

    if (is_valid(pattern) && is_valid(path))
    {
        if (fs::is_regular_file(path))
        {
            log::info("The path is a regular file. Searching...");
            grep_file(path, pattern);
        }
        else
        {
            log::info("The path is a directory. Searching recursively...");
            grep_dir(path, pattern);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        grep(argv[1], argv[2]);
    }
    else
    {
        log::error("Two arguments are required!\n"
                   "Usage: cppgrep <path> <string>, where <path> is a file or "
                   "directory, and <string> is the text you're looking for.");
    }

    return 0;
}
