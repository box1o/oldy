#include <array>
#include <cctype>
#include <algorithm>
#include <string_view>

#include "woki/ext/path_safety.hpp"

namespace woki::ext {

namespace {

[[nodiscard]] bool IsWindowsDeviceName(std::string_view component) {
    const std::size_t extension = component.find('.');
    component = component.substr(0, extension);
    std::string upper(component);
    std::ranges::transform(upper, upper.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    constexpr std::array<std::string_view, 4> kDevices{"CON", "PRN", "AUX", "NUL"};
    if (std::ranges::find(kDevices, upper) != kDevices.end()) {
        return true;
    }
    return upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) && upper.back() >= '1' && upper.back() <= '9';
}

[[nodiscard]] bool IsPortableComponent(std::string_view component) {
    if (component.empty() || component == "." || component == ".." || component.size() > 255 || component.back() == '.' || component.back() == ' ' || IsWindowsDeviceName(component)) {
        return false;
    }
    return std::ranges::none_of(component, [](unsigned char ch) { return ch < 0x20 || ch == 0x7f || ch == '<' || ch == '>' || ch == '"' || ch == '|' || ch == '?' || ch == '*'; });
}

} // namespace

bool HasPathTraversal(const std::filesystem::path& path) {
    return std::ranges::any_of(path, [](const std::filesystem::path& part) { return part == ".."; });
}

bool IsSafeRelativePath(const std::filesystem::path& path) {
    if (path.empty() || path.has_root_path() || HasPathTraversal(path)) {
        return false;
    }

    const std::string portable = path.generic_string();
#ifdef _WIN32
    if (path.native().contains(L'\\')) {
        return false;
    }
#endif
    if (portable.empty() || portable.size() > 4096 || portable.front() == '/' || portable.back() == '/' || portable.contains('\0') || portable.contains('\\') || portable.contains(':') || portable.contains("//")) {
        return false;
    }
    return std::ranges::all_of(path, [](const std::filesystem::path& part) { return IsPortableComponent(part.generic_string()); });
}

} // namespace woki::ext
