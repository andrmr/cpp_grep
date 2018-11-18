#pragma once

#if defined(unix) || defined(__unix__) || defined(__unix)
#    define UNIX_BUILD
#elif (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
#    define WIN32_BUILD
#endif

namespace util::sys {

/// Retrieves the operating system's pagesize value.
long pagesize() noexcept;

#ifdef WIN32_BUILD
/// Provides a reliable read-right check on Windows.
bool win32_can_read(const char* path) noexcept;
#endif

} // namespace util::sys
