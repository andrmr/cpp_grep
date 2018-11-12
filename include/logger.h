#pragma once

#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>

namespace utils::log {

namespace details {

/// Synchronizes stdout output. Usage: SyncPrint{} << message;
class SyncPrint: public std::ostringstream
{
    static std::mutex m_printMutex;

public:
    ~SyncPrint() override
    {
        std::lock_guard<std::mutex> g {m_printMutex};
        std::cout << this->str();
    }
};

std::mutex SyncPrint::m_printMutex {};

/// Formats text input according to printf rules.
/// @returns unique_ptr buffer with the formatted output
/// @todo output ERRFMT if arguments are mismatched
template <typename... Args>
inline auto format(std::string_view format, Args&&... args)
{
    auto size = 1 + std::snprintf(nullptr, 0, format.data(), args...);
    auto buffer = std::make_unique<char[]>(size);
    std::snprintf(buffer.get(), size, format.data(), args...);

    return buffer;
}

/// Prints logs based on type and formats the ouput according to printf rules.
template <typename... Args>
inline void print_log(std::string_view log_type, std::string_view message, Args&&... args)
{
    if constexpr (sizeof...(args) > 0)
    {
        SyncPrint{} << log_type << format(message, std::forward<Args>(args)...).get() << '\n';
    }
    else
    {
        SyncPrint{} << log_type << message << '\n';
    }
}

} // namespace utils::log::details

/// Logs an error message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline void error(std::string_view message, Args&&... args)
{
    details::print_log("Error: ", message, std::forward<Args>(args)...);
}

/// Logs a debug message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline void debug(std::string_view message, Args&&... args)
{
    details::print_log("Debug: ", message, std::forward<Args>(args)...);
}

/// Logs an information message.
/// @param message - message or format with additional arguments
/// @param args - additional arguments used with format
template <typename... Args>
inline void info(std::string_view message, Args&&... args)
{
    details::print_log("Info: ", message, std::forward<Args>(args)...);
}

} // namespace utils::log
