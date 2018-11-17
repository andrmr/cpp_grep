#include <optional>
#include <string>
#include <string_view>

namespace util::misc {

/// Helper class used to return a bool result with an optional error message.
/// By default, initialized as false when an error message is provided.
/// An existing error message is discarded if the value is set to true.
class BoolResult
{
public:
    BoolResult(std::string_view error_msg)
        : m_error_msg {std::string {error_msg}}
    {
    }

    BoolResult(std::string&& error_msg)
        : m_error_msg {std::move(error_msg)}
    {
    }

    BoolResult(const char* error_msg)
        : m_error_msg {std::string {error_msg}}
    {
    }

    BoolResult(bool value)
        : m_value {value}
    {
    }

    // BoolResult(const BoolResult& other)
    // {
    // }
    // BoolResult& operator=(const BoolResult& other)
    // {
    // }

    // BoolResult(BoolResult&& other)
    // {
    // }
    // BoolResult& operator=(BoolResult&& other)
    // {
    // }

    BoolResult& operator=(bool value) & noexcept
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

    operator bool() const noexcept
    {
        return m_value;
    }

    /// Returns reference to the optional error message.
    const std::optional<std::string>& error() const noexcept
    {
        return m_error_msg;
    }

private:
    bool m_value {false};
    std::optional<std::string> m_error_msg {std::nullopt};
};

} // namespace util::misc