#include <cctype>

#include <woki/asset/uri.hpp>

namespace woki::asset {
namespace {

bool ValidUtf8(const std::string_view text) {
    const auto* data = reinterpret_cast<const unsigned char*>(text.data());
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char first = data[i++];
        if (first < 0x80)
            continue;
        unsigned count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
        if (count == 0 || i + count > text.size())
            return false;
        const unsigned char second = data[i];
        if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) || (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
            return false;
        for (unsigned j = 0; j < count; ++j)
            if ((data[i++] & 0xc0U) != 0x80U)
                return false;
    }
    return true;
}

bool ValidAuthority(const std::string_view authority) {
    if (authority.empty())
        return false;
    for (const char item : authority) {
        const auto c = static_cast<unsigned char>(item);
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
            return false;
    }
    return authority != "." && authority != "..";
}

} // namespace

Result<AssetUri> AssetUri::Parse(const std::string_view uri) {
    if (!ValidUtf8(uri) || uri.find('\0') != std::string_view::npos || uri.find('\\') != std::string_view::npos)
        return Err(ErrorCode::InvalidArgument, "asset URI is not normalized UTF-8");
    if (uri.contains('%'))
        return Err(ErrorCode::InvalidArgument, "asset URI must not contain percent-encoded bytes");

    const std::size_t marker = uri.find("://");
    if (marker == std::string_view::npos)
        return Err(ErrorCode::InvalidArgument, "asset URI has no supported scheme");
    const std::string_view scheme_text = uri.substr(0, marker);
    const std::string_view remainder = uri.substr(marker + 3);
    AssetScheme scheme;
    std::string authority;
    std::string_view path_text = remainder;
    if (scheme_text == "engine")
        scheme = AssetScheme::Engine;
    else if (scheme_text == "project")
        scheme = AssetScheme::Project;
    else if (scheme_text == "cache")
        scheme = AssetScheme::Cache;
    else if (scheme_text == "plugin") {
        scheme = AssetScheme::Plugin;
        const std::size_t slash = remainder.find('/');
        if (slash == std::string_view::npos || !ValidAuthority(remainder.substr(0, slash)))
            return Err(ErrorCode::InvalidArgument, "plugin URI requires a normalized package authority and path");
        authority.assign(remainder.substr(0, slash));
        path_text = remainder.substr(slash + 1);
    } else
        return Err(ErrorCode::InvalidArgument, "asset URI scheme is unsupported");
    if (scheme != AssetScheme::Plugin && remainder.starts_with('/'))
        return Err(ErrorCode::InvalidArgument, "asset URI path must not be an absolute host path");
    auto path = AssetPath::Parse(path_text);
    if (!path)
        return Err(std::move(path).error());
    std::string canonical = std::string(scheme_text) + "://";
    if (!authority.empty())
        canonical += authority + '/';
    canonical += path->String();
    if (canonical != uri)
        return Err(ErrorCode::InvalidArgument, "asset URI is not canonical");
    return Ok(AssetUri(scheme, std::move(authority), std::move(*path), std::move(canonical)));
}

AssetUri AssetUri::Engine(const AssetPath& path) {
    return AssetUri(AssetScheme::Engine, {}, path, "engine://" + path.String());
}

} // namespace woki::asset
