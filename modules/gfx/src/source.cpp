#include <woki/gfx/source.hpp>
#include <woki/gfx/compiler.hpp>
#include <woki/rhi.hpp>

#include <algorithm>
#include <set>

namespace woki::gfx {
namespace {

struct IncludeDirective {
    u64 begin;
    u64 end;
    std::string path;
};

std::vector<IncludeDirective> LexIncludes(const std::string_view source) {
    enum class State { Code, String, LineComment, BlockComment };
    State state = State::Code;
    u32 block_depth = 0;
    std::vector<IncludeDirective> result;
    bool line_start = true;
    for (std::size_t i = 0; i < source.size();) {
        if (state == State::LineComment) {
            if (source[i++] == '\n') {
                state = State::Code;
                line_start = true;
            }
            continue;
        }
        if (state == State::BlockComment) {
            if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
                ++block_depth;
                i += 2;
            } else if (i + 1 < source.size() && source[i] == '*' && source[i + 1] == '/') {
                if (--block_depth == 0)
                    state = State::Code;
                i += 2;
            } else {
                line_start = source[i] == '\n';
                ++i;
            }
            continue;
        }
        if (state == State::String) {
            if (source[i] == '\\' && i + 1 < source.size())
                i += 2;
            else if (source[i++] == '"')
                state = State::Code;
            continue;
        }
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            state = State::LineComment;
            i += 2;
            continue;
        }
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            state = State::BlockComment;
            block_depth = 1;
            i += 2;
            continue;
        }
        if (source[i] == '"') {
            state = State::String;
            ++i;
            line_start = false;
            continue;
        }
        if (source[i] == '\n') {
            ++i;
            line_start = true;
            continue;
        }
        if (line_start && (source[i] == ' ' || source[i] == '\t')) {
            ++i;
            continue;
        }
        if (line_start && source.substr(i, 10) == "#include \"") {
            const std::size_t path_begin = i + 10;
            const std::size_t quote = source.find('"', path_begin);
            const std::size_t newline = source.find('\n', path_begin);
            if (quote != std::string_view::npos && (newline == std::string_view::npos || quote < newline)) {
                const std::size_t line_end = newline == std::string_view::npos ? source.size() : newline + 1;
                std::size_t suffix = quote + 1;
                while (suffix < line_end && (source[suffix] == ' ' || source[suffix] == '\t' || source[suffix] == '\r'))
                    ++suffix;
                if (suffix == line_end || (suffix + 1 == line_end && source[suffix] == '\n')) {
                    result.push_back({i, line_end, std::string(source.substr(path_begin, quote - path_begin))});
                    i = line_end;
                    line_start = true;
                    continue;
                }
            }
        }
        line_start = false;
        ++i;
    }
    return result;
}

Result<asset::AssetPath> Resolve(const asset::AssetPath& parent, const std::string_view child) {
    if (child.empty() || child.front() == '/' || child.find('\\') != std::string_view::npos || child.find(':') != std::string_view::npos) {
        return Err(ErrorCode::InvalidArgument, "include must use a local quoted path");
    }
    const std::size_t slash = parent.String().rfind('/');
    const std::string joined = slash == std::string::npos ? std::string(child) : parent.String().substr(0, slash + 1) + std::string(child);
    return asset::AssetPath::Parse(joined);
}

class Composer {
public:
    explicit Composer(const asset::Vfs& vfs)
        : vfs_(vfs) {}

    void File(const asset::AssetPath& path, ComposedSource& output, const std::optional<SourceRange>& include_site = std::nullopt) {
        const auto cycle = std::ranges::find(stack_, path);
        if (cycle != stack_.end()) {
            ShaderDiagnostic diagnostic{"SHD2003", DiagnosticSeverity::Error, "shader include cycle detected", include_site.value_or(SourceRange{path, 0, 0, 0, 0}), {}};
            for (auto it = cycle; it != stack_.end(); ++it)
                diagnostic.notes.push_back({"included from here", SourceRange{*it, 0, 0, 0, 0}});
            output.diagnostics.push_back(std::move(diagnostic));
            return;
        }
        if (seen_.contains(path))
            return;
        auto source = vfs_.ReadText(path);
        if (!source) {
            output.diagnostics.push_back({"SHD2001", DiagnosticSeverity::Error, std::string(source.error().Message()), include_site.value_or(SourceRange{path, 0, 0, 0, 0}), {}});
            return;
        }
        seen_.insert(path);
        output.dependencies.push_back(path);
        stack_.push_back(path);
        const auto includes = LexIncludes(*source);
        u64 cursor = 0;
        for (const auto& include : includes) {
            Append(path, *source, cursor, include.begin, output);
            auto child = Resolve(path, include.path);
            if (!child)
                output.diagnostics.push_back({"SHD2002", DiagnosticSeverity::Error, std::string(child.error().Message()), SourceRange{path, include.begin, include.end - include.begin, 0, 0}, {}});
            else
                File(*child, output, SourceRange{path, include.begin, include.end - include.begin, 0, 0});
            cursor = include.end;
        }
        Append(path, *source, cursor, source->size(), output);
        stack_.pop_back();
    }

private:
    static void Append(const asset::AssetPath& path, const std::string& source, const u64 begin, const u64 end, ComposedSource& output) {
        if (begin == end)
            return;
        const u64 generated = output.code.size();
        output.code.append(source, static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
        output.source_map.push_back({generated, generated + end - begin, path, begin, end});
    }

    const asset::Vfs& vfs_;
    std::vector<asset::AssetPath> stack_;
    std::set<asset::AssetPath> seen_;
};

} // namespace

ComposedSource ShaderSourceResolver::Compose(const ShaderDescriptor& descriptor, std::optional<asset::AssetPath> descriptor_path) const {
    ComposedSource result;
    Composer composer(vfs_);
    if (descriptor_path)
        result.dependencies.push_back(std::move(*descriptor_path));
    for (const auto& source : descriptor.sources) {
        composer.File(source, result);
        if (!result.code.empty() && result.code.back() != '\n')
            result.code.push_back('\n');
    }
    std::ranges::sort(result.dependencies);
    result.dependencies.erase(std::ranges::unique(result.dependencies).begin(), result.dependencies.end());
    return result;
}

SourceRange MapGeneratedRange(const ComposedSource& source, const u64 offset, const u64 length) {
    for (const auto& entry : source.source_map) {
        if (offset >= entry.generated_begin && offset < entry.generated_end) {
            const u64 local = offset - entry.generated_begin;
            return {entry.source, entry.source_begin + local, std::min(length, entry.generated_end - offset), 0, 0};
        }
    }
    return {{}, offset, length, 0, 0};
}

Result<ref<rhi::ShaderModule>> CreateShaderModuleFromAsset(rhi::Device& device, const asset::Vfs& vfs, const asset::AssetPath& descriptor_path) {
    std::string descriptor_text;
    TRY_ASSIGN(descriptor_text, vfs.ReadText(descriptor_path));
    const DescriptorResult parsed = ParseShaderDescriptor(descriptor_path, descriptor_text);
    if (!parsed.diagnostics.empty())
        return Err(ErrorCode::InvalidArgument, parsed.diagnostics.front().message);
    ComposedSource composed = ShaderSourceResolver(vfs).Compose(parsed.descriptor, descriptor_path);
    if (!composed.diagnostics.empty())
        return Err(ErrorCode::InvalidArgument, composed.diagnostics.front().message);

    std::string code = composed.code;
    if (IsTintShaderCompilerAvailable()) {
        auto compiler = CreateTintShaderCompiler();
        const CompileOutput compiled = compiler->Compile({parsed.descriptor, composed});
        const auto error = std::ranges::find_if(compiled.diagnostics, [](const ShaderDiagnostic& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
        if (error != compiled.diagnostics.end())
            return Err(ErrorCode::InvalidArgument, error->message);
        code = compiled.code;
    }
    return device.CreateShaderModule({.code = code, .label = parsed.descriptor.name});
}

} // namespace woki::gfx
