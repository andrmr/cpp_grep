#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

#include "util/thread_pool.h"

namespace cppgrep {

constexpr auto MAX_PATTERN_SIZE {128U}; //!< Max pattern size, in characters.
constexpr auto MAX_AFFIX_SIZE {3U};     //!< Max affix size, in characters.

class Grep
{
public:
    /// Builds a Grep instance if the arguments are valid, or throws otherwise.
    /// @param path - the path where to search
    /// @param pattern - the text pattern to search for
    /// @param threads - number of threads to use
    /// @returns Grep instance
    /// @throws std::invalid_arguments
    static Grep build_grep(std::string_view path, std::string_view pattern, unsigned int threads = 0);

    /// Starts the search on a Grep object.
    /// @returns the number of results.
    unsigned long search() noexcept;

private:
    explicit Grep(std::string_view path, std::string_view pattern, unsigned int threads = 0);

    /// Recursively iterates a directory and searches a text pattern in each valid file.
    void grep_dir(const std::filesystem::path& dir_path);

    /// Searches a text pattern in a file.
    void grep_file(const std::filesystem::path& file_path, bool single_file = false);

    /// Searches a text pattern in a buffer.
    void grep_chunk(const std::vector<char>& chunk, unsigned long offset, unsigned long chunk_count, std::shared_ptr<std::string> file_name);

    std::string m_pattern;
    std::filesystem::path m_path;
    std::boyer_moore_searcher<std::string_view::const_iterator> m_searcher;
    unsigned long m_chunk_size;
    unsigned long m_increment;
    std::unique_ptr<util::misc::ThreadPool> m_threadpool;
    std::atomic_long m_result_count {0};
};

} // namespace cppgrep
