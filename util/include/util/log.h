#pragma once

#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>

namespace util {

/// String format utilities.
namespace fmt {

/// Formats text input according to printf rules.
/// @returns buffer holding the formatted output
/// @todo output ERRFMT if arguments are mismatched
template <typename... Args>
inline auto format_buf(std::string_view fmt, Args&&... args) -> std::unique_ptr<char[]>
{
    auto size   = 1 + std::snprintf(nullptr, 0, fmt.data(), args...);
    auto buffer = std::make_unique<char[]>(size);
    std::snprintf(buffer.get(), size, fmt.data(), args...);

    return buffer;
}

/// Formats text input according to printf rules.
/// @returns string_view into a buffer holding the formatted output
template <typename... Args>
inline auto format_str(std::string_view fmt, Args&&... args) -> std::string
{
    return {format_buf(fmt, std::forward<Args>(args)...).get()};
}

} // namespace fmt


/// Logging utilities.
namespace log {

namespace impl {

static std::mutex print_mutex {}; //!< Used to synchronize stdout.

/// Prints logs based on type and formats the output according to printf rules.
template <typename... Args>
inline auto print_log(std::string_view log_type, std::string_view message, Args&&... args)
{
    std::ostringstream print_buffer;

    if constexpr (sizeof...(args) > 0)
    {
        print_buffer << log_type << fmt::format_buf(message, std::forward<Args>(args)...).get() << '\n';
    }
    else
    {
        print_buffer << log_type << message << '\n';
    }

    std::lock_guard g {print_mutex};
    std::cout << print_buffer.str();
}

} // namespace impl

/// Logs an error message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline constexpr auto error(std::string_view message, Args&&... args)
{
    impl::print_log("Error: ", message, std::forward<Args>(args)...);
}

/// Logs a debug message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline constexpr auto debug(std::string_view message, Args&&... args)
{
    impl::print_log("Debug: ", message, std::forward<Args>(args)...);
}

/// Logs an information message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline constexpr auto info(std::string_view message, Args&&... args)
{
    impl::print_log("Info: ", message, std::forward<Args>(args)...);
}

} // namespace log
} // namespace util
