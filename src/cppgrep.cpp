#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <vector>

#include "logger.h"
#include "thread_pool.h"

namespace fs = std::filesystem;
using namespace utils;

static constexpr auto DEFAULT_CHUNK_SIZE {262144L}; //!< Default chunk size, in bytes.
static constexpr auto MAX_PATTERN_SIZE {128U};      //!< Max pattern size, in characters.

void grep_file(const fs::path& file_path, std::string_view pattern)
{
    // todo: avoid a thread pool when grepping a single file which fits into a single chunk
    // todo: restrict search within chunk limits for the prefix/suffix requirement
    // todo: move the lambda's logic and the state it captures into a class, and process objects instead
    // todo: limit the total amount of memory to queue as chunks; block the read thread if buffer is full

    static utils::tp::ThreadPool thread_pool {};
    static const auto searcher   = std::boyer_moore_searcher(pattern.begin(), pattern.end());
    static const auto chunk_size = std::max(static_cast<decltype(DEFAULT_CHUNK_SIZE)>(pattern.size()), DEFAULT_CHUNK_SIZE);

    if (std::ifstream stream {file_path}; stream.good())
    {
        auto chunk_count {0UL};
        do
        {
            auto chunk = std::vector<char>(chunk_size);
            stream.read(&chunk[0], chunk_size);

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
            ++chunk_count;
        } while (stream.gcount() == chunk_size);
    }
}

void grep_dir(const fs::path& dir_path, std::string_view pattern)
{
    // note: range-for recursive iterator throws, if permission is denied

    std::error_code ec;
    for (fs::recursive_directory_iterator it {dir_path}, end; it != end; it.increment(ec))
    {
        if (fs::is_regular_file(it->path()))
        {
            grep_file(it->path(), pattern);
        }
    }
}

bool validate_pattern(std::string_view pattern)
{
    if (pattern.size() > MAX_PATTERN_SIZE)
    {
        log::error("Pattern size exceeds the limit: %u.", MAX_PATTERN_SIZE);
        return false;
    }

    return true;
}

bool validate_path(const fs::path& path)
{
    try
    {
        if (!fs::exists(path))
        {
            log::error("The path doesn't exist!");
            return false;
        }
    }
    catch (fs::filesystem_error& e)
    {
        if (e.code() == std::errc::permission_denied)
        {
            log::error("Permission denied when accessing the path!");
        }
        else
        {
            log::error("Unable to access the path. Reason: %s", e.what());
        }

        return false;
    }

    if (!fs::is_regular_file(path) && !fs::is_directory(path))
    {
        log::error("The path must be a regular file or a directory!");
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        fs::path path {argv[1]};
        std::string_view pattern {argv[2]};

        if (validate_pattern(pattern) && validate_path(path))
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
    else
    {
        log::error("Two arguments are required!\n"
                   "Usage: cppgrep <path> <string>, where <path> is a file or "
                   "directory, and <string> is the text you're looking for.");
    }

    return 0;
}
