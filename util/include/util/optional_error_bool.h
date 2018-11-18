#include <optional>
#include <string>
#include <string_view>

namespace util::misc {

/// Helper class used to return a bool result with an optional error message.
/// By default, initialized as false when an error message is provided.
/// An existing error message is discarded if the value is set to true.
class OptionalErrorBool
{
public:
    OptionalErrorBool(std::string_view error_msg);

    OptionalErrorBool(std::string&& error_msg);

    OptionalErrorBool(const char* error_msg);

    OptionalErrorBool(bool value);

    OptionalErrorBool& operator=(bool value) & noexcept;

    operator bool() const noexcept;

    /// Returns reference to the optional error message.
    const std::optional<std::string>& error() const noexcept;

private:
    bool m_value {false};
    std::optional<std::string> m_error_msg {std::nullopt};
};

} // namespace util::misc
