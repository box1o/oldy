#include <woki/asset/path.hpp>

#include <cstddef>

namespace woki::asset {

Result<AssetPath> AssetPath::Parse(const std::string_view path) {
    if (path.empty()) {
        return Err(ErrorCode::InvalidArgument, "asset path is empty");
    }
    if (path.front() == '/' || path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos) {
        return Err(ErrorCode::InvalidArgument, "asset path must be a relative virtual path");
    }

    std::string normalized;
    normalized.reserve(path.size());
    std::size_t position = 0;
    while (position < path.size()) {
        const std::size_t separator = path.find('/', position);
        const std::size_t end = separator == std::string_view::npos ? path.size() : separator;
        const std::string_view component = path.substr(position, end - position);
        if (component == "..") {
            return Err(ErrorCode::InvalidArgument, "asset path cannot traverse its virtual root");
        }
        if (!component.empty() && component != ".") {
            if (component.find(':') != std::string_view::npos) {
                return Err(ErrorCode::InvalidArgument, "asset path contains an invalid component");
            }
            if (!normalized.empty()) {
                normalized.push_back('/');
            }
            normalized.append(component);
        }
        position = end + 1;
    }

    if (normalized.empty()) {
        return Err(ErrorCode::InvalidArgument, "asset path does not name a file");
    }
    return Ok(AssetPath(std::move(normalized)));
}

} // namespace woki::asset
