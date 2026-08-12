#pragma once

#include <string>
#include <optional>
#include <vector>

#include <woki/asset.hpp>
#include <woki/core.hpp>

namespace woki::gfx {

struct ShaderAssetTag;
struct ShaderVariantTag;
using ShaderAssetHandle = Handle<ShaderAssetTag>;
using ShaderVariantHandle = Handle<ShaderVariantTag>;

enum class ShaderState : u8 { Empty, Loading, Ready, Failed, Reloading };
enum class ShaderLanguage : u8 { Wgsl };
enum class ShaderStage : u8 { Vertex, Fragment, Compute };
enum class DiagnosticSeverity : u8 { Note, Warning, Error };

struct SourceRange {
    std::optional<asset::AssetPath> path;
    u64 byte_offset{0};
    u64 byte_length{0};
    u32 line{0};
    u32 column{0};
};

struct DiagnosticNote {
    std::string message;
    SourceRange range;
};

struct ShaderDiagnostic {
    std::string code;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    SourceRange range;
    std::vector<DiagnosticNote> notes;
};

struct ShaderAssetRecord {
    ShaderState state{ShaderState::Empty};
    u64 version{0};
    ContentHash product_hash;
    std::vector<ShaderDiagnostic> diagnostics;
};

struct ShaderVariantRecord {
    ShaderState state{ShaderState::Empty};
    u64 version{0};
    ContentHash variant_hash;
    ContentHash product_hash;
    std::vector<ShaderDiagnostic> diagnostics;
};

} // namespace woki::gfx
