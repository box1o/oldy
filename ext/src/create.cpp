#include <cctype>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <unordered_set>

#include <woki/ext/manifest.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

struct RenderedFile {
    fs::path destination;
    std::string contents;
};

[[nodiscard]] std::string Slug(std::string_view text, char separator) {
    std::string out;
    bool previous_separator = false;
    for (const char raw_ch : text) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
            previous_separator = false;
            continue;
        }
        if (!previous_separator && !out.empty()) {
            out.push_back(separator);
            previous_separator = true;
        }
    }
    while (!out.empty() && out.back() == separator)
        out.pop_back();
    return out.empty() ? "extension" : out;
}

[[nodiscard]] std::string ToId(std::string_view name) {
    return "woki." + Slug(name, '.');
}

[[nodiscard]] std::string QuoteYaml(std::string_view value) {
    std::string quoted(value);
    std::size_t position = 0;
    while ((position = quoted.find('\'', position)) != std::string::npos) {
        quoted.insert(position, 1, '\'');
        position += 2;
    }
    return quoted;
}

[[nodiscard]] bool SafeRelativePath(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_path())
        return false;
    for (const fs::path& component : path) {
        if (component.empty() || component == "." || component == "..")
            return false;
    }
    return true;
}

void WriteFile(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.good())
        throw std::runtime_error("Failed to create file: " + path.string());
    output << contents;
    output.close();
    if (!output)
        throw std::runtime_error("Failed to write file: " + path.string());
}

} // namespace

std::expected<std::string, std::string> RenderTemplate(std::string_view contents, const TemplateReplacements& replacements) {
    std::string output;
    output.reserve(contents.size());
    std::size_t cursor = 0;
    while (cursor < contents.size()) {
        const std::size_t opening = contents.find("{{", cursor);
        const std::size_t unexpected_close = contents.find("}}", cursor);
        if (unexpected_close != std::string_view::npos && (opening == std::string_view::npos || unexpected_close < opening))
            return std::unexpected("Malformed template placeholder");
        if (opening == std::string_view::npos) {
            output.append(contents.substr(cursor));
            break;
        }
        output.append(contents.substr(cursor, opening - cursor));
        const std::size_t closing = contents.find("}}", opening + 2);
        if (closing == std::string_view::npos)
            return std::unexpected("Malformed template placeholder");
        const std::string name(contents.substr(opening + 2, closing - opening - 2));
        if (name.empty() || !std::ranges::all_of(name, [](const char raw_ch) {
                const auto ch = static_cast<unsigned char>(raw_ch);
                return std::isupper(ch) != 0 || std::isdigit(ch) != 0 || ch == '_';
            }))
            return std::unexpected("Malformed template placeholder: {{" + name + "}}");
        const auto replacement = replacements.find(name);
        if (replacement == replacements.end())
            return std::unexpected("Unknown template placeholder: {{" + name + "}}");
        output += replacement->second;
        cursor = closing + 2;
    }
    if (output.find("{{") != std::string::npos || output.find("}}") != std::string::npos)
        return std::unexpected("Unresolved template placeholder");
    return output;
}

std::expected<void, std::string> InstantiateTemplates(const fs::path& template_dir, const fs::path& destination, const TemplateReplacements& replacements) {
    std::error_code error;
    if (!fs::is_directory(template_dir, error) || error)
        return std::unexpected("Template directory is unavailable: " + template_dir.string());
    const fs::file_status destination_status = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory)
        return std::unexpected("Cannot inspect template destination: " + error.message());
    if (!error && destination_status.type() != fs::file_type::not_found)
        return std::unexpected("Template destination already exists: " + destination.string());
    error.clear();

    std::vector<RenderedFile> files;
    std::unordered_set<std::string> destinations;
    for (fs::recursive_directory_iterator it(template_dir, fs::directory_options::none, error), end; it != end; it.increment(error)) {
        if (error)
            return std::unexpected("Failed to inspect templates: " + error.message());
        if (it->is_symlink(error))
            return std::unexpected("Template entries must not be symbolic links: " + it->path().string());
        if (error)
            return std::unexpected("Failed to inspect template entry: " + error.message());
        if (it->is_directory())
            continue;
        if (!it->is_regular_file())
            return std::unexpected("Template entry is not a regular file: " + it->path().string());

        fs::path relative = it->path().lexically_relative(template_dir);
        if (relative.extension() == ".in")
            relative.replace_extension();
        auto rendered_path = RenderTemplate(relative.generic_string(), replacements);
        if (!rendered_path)
            return std::unexpected(rendered_path.error());
        relative = fs::path(*rendered_path);
        if (!SafeRelativePath(relative))
            return std::unexpected("Unsafe template destination: " + relative.string());
        if (!destinations.insert(relative.generic_string()).second)
            return std::unexpected("Duplicate template destination: " + relative.string());

        std::ifstream input(it->path(), std::ios::binary);
        if (!input)
            return std::unexpected("Failed to read template: " + it->path().string());
        const std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        auto rendered = RenderTemplate(contents, replacements);
        if (!rendered)
            return std::unexpected(it->path().string() + ": " + rendered.error());
        files.push_back({destination / relative, std::move(*rendered)});
    }
    if (files.empty())
        return std::unexpected("Template directory is empty: " + template_dir.string());

    try {
        for (const auto& file : files)
            WriteFile(file.destination, file.contents);
    } catch (const std::exception& exception) {
        std::error_code cleanup_error;
        fs::remove_all(destination, cleanup_error);
        return std::unexpected(exception.what());
    }
    return {};
}

Status Create(Context& context, const CreateOptions& options) {
    if (options.name.empty()) {
        context.diagnostics.Error("Extension name is required");
        return Status::Usage;
    }
    if (options.lang != "c" && options.lang != "cpp") {
        context.diagnostics.Error("--lang must be c or cpp");
        return Status::Usage;
    }

    const std::string dir_name = Slug(options.name, '-');
    const std::string id = options.id.empty() ? ToId(options.name) : options.id;
    if (!woki::ext::IsValidExtensionId(id)) {
        context.diagnostics.Error("Extension id must use lowercase reverse-DNS-style segments, for example woki.hello");
        return Status::Usage;
    }
    const fs::path root = options.out_dir / dir_name;
    std::error_code error;
    if (context.filesystem.IsSymlink(root, error)) {
        context.diagnostics.Err() << "Refusing unsafe symbolic-link destination: " << root << '\n';
        return Status::Error;
    }
    if (error == std::errc::no_such_file_or_directory)
        error.clear();
    if (error) {
        context.diagnostics.Error(error.message());
        return Status::Error;
    }
    if (context.filesystem.Exists(root, error)) {
        context.diagnostics.Err() << "Refusing to overwrite existing directory: " << root << '\n';
        return Status::Error;
    }
    if (error) {
        context.diagnostics.Error(error.message());
        return Status::Error;
    }

    const fs::path templates = FindTemplateDir(options.executable);
    if (templates.empty()) {
        context.diagnostics.Error("Cannot locate Woki extension project templates");
        return Status::Error;
    }
    const TemplateReplacements replacements{{"ID", id}, {"NAME", QuoteYaml(options.name)}};
    auto instantiated = InstantiateTemplates(templates / options.lang, root, replacements);
    if (!instantiated) {
        context.diagnostics.Error(instantiated.error());
        return Status::Error;
    }

    context.diagnostics.Out() << "Created extension project: " << root << '\n';
    return Status::Ok;
}

} // namespace wokiext
