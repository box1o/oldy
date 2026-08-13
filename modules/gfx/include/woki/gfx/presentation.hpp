#pragma once

#include <string>
#include <memory>
#include <variant>

#include <woki/core.hpp>

namespace woki::gfx {

struct SurfaceTag;
struct OffscreenTargetTag;
using SurfaceHandle = Handle<SurfaceTag>;
using OffscreenTargetHandle = Handle<OffscreenTargetTag>;

enum class PixelFormat : u8 { BGRA8Unorm, RGBA8Unorm, RGBA8UnormSrgb, RGBA16Float, R32Uint };
enum class PresentMode : u8 { Fifo, Mailbox, Immediate };

enum class SurfacePlatform : u8 { WokiWindow, Win32, Xlib, Wayland, MetalLayer, HtmlCanvas };

struct SurfacePlatformSource final {
    SurfacePlatform platform{SurfacePlatform::WokiWindow};
    void* display{};
    void* window{};
    std::string selector;
};

class SurfaceSource {
public:
    virtual ~SurfaceSource() = default;
    [[nodiscard]] virtual SurfacePlatformSource Describe() const noexcept = 0;
    [[nodiscard]] virtual std::string Label() const = 0;
};

struct SurfaceDescriptor final {
    ref<const SurfaceSource> source;
    u32 width{};
    u32 height{};
    PresentMode present_mode{PresentMode::Fifo};
    std::string label;
};

enum class SurfaceState : u8 { Ready, Minimized, Outdated, Lost, Failed };

struct OffscreenTargetDescriptor final {
    u32 width{1};
    u32 height{1};
    PixelFormat format{PixelFormat::RGBA8Unorm};
    u32 sample_count{1};
    bool sampled{true};
    bool exportable{};
    std::string label;
};

struct SurfaceOutput final {
    SurfaceHandle surface;
};

struct OffscreenOutput final {
    OffscreenTargetHandle target;
};

struct ReadbackOutput final {
    OffscreenTargetHandle target;
};

using ViewOutput = std::variant<SurfaceOutput, OffscreenOutput, ReadbackOutput>;

} // namespace woki::gfx
