#pragma once

#include <functional>

#include <woki/gfx/advanced/buffer_pool.hpp>
#include <woki/gfx/canvas.hpp>
#include <woki/gfx/advanced/graph_executor.hpp>
#include <woki/gfx/advanced/layout.hpp>
#include <woki/gfx/advanced/library.hpp>
#include <woki/gfx/advanced/texture_library.hpp>
#include <woki/gfx/advanced/upload.hpp>

namespace woki::gfx {

struct CanvasResolvedImage final {
    ref<rhi::Texture> texture;
    ref<rhi::TextureView> view;
    TextureHandle logical_texture;
};

struct CanvasTarget final {
    ref<rhi::Texture> texture;
    ref<rhi::TextureView> view;
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    u32 width{};
    u32 height{};
};

struct CanvasSubmission final {
    rhi::SubmissionTicket submission;
    std::vector<TextureHandle> textures;
};

class CanvasFeature final {
public:
    using ImageResolver = std::function<Result<CanvasResolvedImage>(const CanvasImageSource&)>;

    [[nodiscard]] static Result<scope<CanvasFeature>> Create(ref<rhi::Device> device, UploadScheduler& uploads, ref<DeferredReleaseQueue> releases, const asset::Product& shader_product);
    ~CanvasFeature();
    CanvasFeature(const CanvasFeature&) = delete;
    CanvasFeature& operator=(const CanvasFeature&) = delete;
    [[nodiscard]] Result<CanvasSubmission> Execute(const CanvasFrame& canvas, const CanvasTarget& target, const ImageResolver& resolve);
    void MarkDeviceLost() noexcept;

private:
    struct Impl;
    explicit CanvasFeature(scope<Impl> impl);
    scope<Impl> impl_;
};

} // namespace woki::gfx
