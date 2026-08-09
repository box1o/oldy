#include <string>
#include <iostream>
#include <filesystem>

#include <woki/ext/ext.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string JsonEscape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                const auto byte = static_cast<unsigned char>(ch);
                if (byte < 0x20) {
                    out += "\\u00";
                    constexpr char kHex[] = "0123456789abcdef";
                    out.push_back(kHex[(byte >> 4) & 0x0f]);
                    out.push_back(kHex[byte & 0x0f]);
                } else {
                    out.push_back(ch);
                }
                break;
        }
    }
    return out;
}

void PrintCommandJsonObject(std::string_view extension_id, const woki::ext::CommandContribution& command) {
    std::cout << "{\"extension\":\"" << JsonEscape(extension_id) << "\",\"id\":\"" << JsonEscape(command.id) << "\",\"title\":\"" << JsonEscape(command.title) << "\",\"category\":\"" << JsonEscape(command.category)
              << "\"}";
}

Status CommandsForPath(const fs::path& path, bool json) {
    const fs::path root = fs::absolute(path).lexically_normal();
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        std::cerr << manifest.error().Message() << '\n';
        return Status::Error;
    }

    if (json) {
        std::cout << "[";
        for (std::size_t i = 0; i < manifest->commands.size(); ++i) {
            if (i != 0) {
                std::cout << ",";
            }
            PrintCommandJsonObject(manifest->id, manifest->commands[i]);
        }
        std::cout << "]\n";
        return Status::Ok;
    }

    for (const woki::ext::CommandContribution& command : manifest->commands) {
        std::cout << command.id << " " << command.title;
        if (!command.category.empty()) {
            std::cout << " [" << command.category << "]";
        }
        std::cout << '\n';
    }
    return Status::Ok;
}

} // namespace

Status Commands(const CommandsOptions& options) {
    if (!options.path.empty()) {
        return CommandsForPath(options.path, options.json);
    }

    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        std::cerr << roots.error().Message() << '\n';
        return Status::Error;
    }

    woki::ext::Manager manager;
    manager.SetRoots(*roots);
    auto scanned = manager.Scan();
    if (!scanned) {
        std::cerr << scanned.error().Message() << '\n';
        return Status::Error;
    }

    if (options.json) {
        std::cout << "[";
        bool first = true;
        for (const woki::ext::CommandRecord& record : manager.Commands().Records()) {
            if (!first) {
                std::cout << ",";
            }
            PrintCommandJsonObject(record.extension_id, record.command);
            first = false;
        }
        std::cout << "]\n";
        return Status::Ok;
    }

    for (const woki::ext::CommandRecord& record : manager.Commands().Records()) {
        std::cout << record.command.id << " " << record.command.title;
        if (!record.command.category.empty()) {
            std::cout << " [" << record.command.category << "]";
        }
        std::cout << '\n';
    }
    return Status::Ok;
}

} // namespace wokiext
