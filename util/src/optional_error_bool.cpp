#include "util/optional_error_bool.h"

using namespace util::misc;

OptionalErrorBool::OptionalErrorBool(const OptionalErrorBool& other) noexcept
    : m_value {other.m_value}, m_error_msg {other.m_error_msg}
{
}

OptionalErrorBool& OptionalErrorBool::operator=(const OptionalErrorBool& other) & noexcept
{
    if (this != &other)
    {
        m_value     = other.m_value;
        m_error_msg = other.m_error_msg;
    }

    return *this;
}

OptionalErrorBool::OptionalErrorBool(OptionalErrorBool&& other) noexcept
    : m_value {other.m_value}, m_error_msg {std::move(other.m_error_msg)}
{
}

OptionalErrorBool& OptionalErrorBool::operator=(OptionalErrorBool&& other) noexcept
{
    if (this != &other)
    {
        m_value     = other.m_value;
        m_error_msg = std::move(other.m_error_msg);
    }

    return *this;
}

OptionalErrorBool::OptionalErrorBool(std::string_view error_msg) noexcept
    : m_error_msg {std::string {error_msg}}
{
}

OptionalErrorBool::OptionalErrorBool(std::string&& error_msg) noexcept
    : m_error_msg {std::move(error_msg)}
{
}

OptionalErrorBool::OptionalErrorBool(const char* error_msg) noexcept
    : m_error_msg {std::string {error_msg}}
{
}

OptionalErrorBool::OptionalErrorBool(bool value) noexcept
    : m_value {value}
{
}

OptionalErrorBool& OptionalErrorBool::operator=(bool value) & noexcept
{
    if (m_value != value)
    {
        m_value = value;

        // true doesn't don't carry error message
        if (value && m_error_msg.has_value())
        {
            m_error_msg = std::nullopt;
        }
    }

    return *this;
}

OptionalErrorBool::operator bool() const noexcept
{
    return m_value;
}

const std::optional<std::string>& OptionalErrorBool::error() const noexcept
{
    return m_error_msg;
}
