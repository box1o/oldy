#include <string>
#include <filesystem>

#include <woki/ext/manifest.hpp>
#include <woki/ext/host/factory.hpp>

#include "cli_internal.hpp"

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

void PrintCommandJsonObject(Context& context, std::string_view extension_id, const woki::ext::CommandContribution& command) {
    context.diagnostics.Out() << "{\"extension\":\"" << JsonEscape(extension_id) << "\",\"id\":\"" << JsonEscape(command.id) << "\",\"title\":\"" << JsonEscape(command.title) << "\",\"category\":\""
                              << JsonEscape(command.category) << "\"}";
}

Status CommandsForPath(Context& context, const fs::path& path, bool json) {
    const fs::path root = fs::absolute(path).lexically_normal();
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        context.diagnostics.Error(manifest.error().Message());
        return Status::Error;
    }

    if (json) {
        context.diagnostics.Out() << "[";
        for (std::size_t i = 0; i < manifest->commands.size(); ++i) {
            if (i != 0) {
                context.diagnostics.Out() << ",";
            }
            PrintCommandJsonObject(context, manifest->id, manifest->commands[i]);
        }
        context.diagnostics.Out() << "]\n";
        return Status::Ok;
    }

    for (const woki::ext::CommandContribution& command : manifest->commands) {
        context.diagnostics.Out() << command.id << " " << command.title;
        if (!command.category.empty()) {
            context.diagnostics.Out() << " [" << command.category << "]";
        }
        context.diagnostics.Out() << '\n';
    }
    return Status::Ok;
}

} // namespace

Status Commands(Context& context, const CommandsOptions& options) {
    if (!options.path.empty()) {
        return CommandsForPath(context, options.path, options.json);
    }

    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    auto manager = woki::ext::CreateExtensionManager();
    manager->SetRoots(*roots);
    auto scanned = manager->Scan();
    if (!scanned) {
        context.diagnostics.Error(scanned.error().Message());
        return Status::Error;
    }

    if (options.json) {
        context.diagnostics.Out() << "[";
        bool first = true;
        for (const woki::ext::CommandRecord& record : manager->Commands()) {
            if (!first) {
                context.diagnostics.Out() << ",";
            }
            PrintCommandJsonObject(context, record.extension_id, record.command);
            first = false;
        }
        context.diagnostics.Out() << "]\n";
        return Status::Ok;
    }

    for (const woki::ext::CommandRecord& record : manager->Commands()) {
        context.diagnostics.Out() << record.command.id << " " << record.command.title;
        if (!record.command.category.empty()) {
            context.diagnostics.Out() << " [" << record.command.category << "]";
        }
        context.diagnostics.Out() << '\n';
    }
    return Status::Ok;
}

} // namespace wokiext
