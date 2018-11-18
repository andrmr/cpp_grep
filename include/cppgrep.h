#pragma once

#include "util/optional_error_bool.h"
#include "util/thread_pool.h"

namespace cppgrep {

constexpr auto MAX_PATTERN_SIZE {128U}; //!< Max pattern size, in characters.
constexpr auto MAX_AFFIX_SIZE {3U};     //!< Max affix size, in characters.

using opt_err = util::misc::OptionalErrorBool; //!< Return type that acts as a bool but can hold an error message.

/// Searches a text pattern in a given filesystem path. Optionally, it may use a thread pool.
opt_err grep(std::string_view path, std::string_view pattern, util::misc::ThreadPool::Ptr thread_pool = nullptr) noexcept;

} // namespace cppgrep
