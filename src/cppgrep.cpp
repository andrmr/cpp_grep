#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

#include "logger.h"
#include "thread_pool.h"

namespace fs = std::filesystem;
using namespace utils;

static constexpr auto DEFAULT_CHUNK_SIZE {262144L}; //!< Default chunk size, in bytes.
static constexpr auto MAX_PATTERN_SIZE {128U};      //!< Max pattern size, in characters.

void grep_file(const fs::path &file_path, std::string_view pattern)
{
    // todo: avoid a thread pool when grepping a single file which fits into a chunk
    // todo: restrict search within chunk limits for the prefix/suffix requirement

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
        }
        while (stream.gcount() == chunk_size);
    }
}

void grep_dir(const fs::path &dir_path, std::string_view pattern)
{
    // note: range-for doesn't work if permission is denied

    std::error_code ec;
    for (fs::recursive_directory_iterator it {dir_path}, end; it != end; it.increment(ec))
    {
        if (fs::is_regular_file(it->path()))
        {
            grep_file(it->path(), pattern);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        if (std::string_view pattern {argv[2]}; pattern.size() < MAX_PATTERN_SIZE)
        {
            switch (fs::path path {argv[1]}; fs::status(path).type())
            {
                case fs::file_type::regular:
                    log::info("The path is a file. Searching...");
                    grep_file(path, pattern);
                    break;
                
                case fs::file_type::directory:
                    log::info("The path is a directory. Searching recursively...");
                    grep_dir(path, pattern);
                    break;
                
                case fs::file_type::none:
                case fs::file_type::not_found:
                case fs::file_type::unknown:
                    log::error("Invalid path!");
                    break;
                
                default:
                    log::error("Unsupported file type!");
            }
        }
        else
        {
            log::error("Pattern size exceeds the limit: %u.", MAX_PATTERN_SIZE);
        }
    }
    else
    {
        log::error("Invalid arguments!\n"
                   "Usage: cppgrep <path> <string>, where <path> is a file or "
                   "directory, and <string> is the text you're looking for.");
    }

    return 0;
}
