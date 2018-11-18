#include "util/optional_error_bool.h"

using namespace util::misc;

OptionalErrorBool::OptionalErrorBool(std::string_view error_msg)
    : m_error_msg {std::string {error_msg}}
{
}

OptionalErrorBool::OptionalErrorBool(std::string&& error_msg)
    : m_error_msg {std::move(error_msg)}
{
}

OptionalErrorBool::OptionalErrorBool(const char* error_msg)
    : m_error_msg {std::string {error_msg}}
{
}

OptionalErrorBool::OptionalErrorBool(bool value)
    : m_value {value}
{
}

OptionalErrorBool& OptionalErrorBool::operator=(bool value) & noexcept
{
    if (m_value != value)
    {
        m_value = value;

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
