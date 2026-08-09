#pragma once

#include <woki/core.hpp>

#include "objects.hpp"

namespace woki::rhi {

class Frame {
public:
    [[nodiscard]] TextureView& ColorView();
    [[nodiscard]] const TextureView& ColorView() const;
    [[nodiscard]] const ref<TextureView>& ColorViewRef() const noexcept;
    [[nodiscard]] TextureView* DepthView() noexcept;
    [[nodiscard]] const TextureView* DepthView() const noexcept;

    [[nodiscard]] u32 Width() const noexcept {
        return width_;
    }

    [[nodiscard]] u32 Height() const noexcept {
        return height_;
    }

    [[nodiscard]] f32 AspectRatio() const noexcept {
        return height_ != 0 ? static_cast<f32>(width_) / static_cast<f32>(height_) : 1.0f;
    }

private:
    friend class Swapchain;

    ref<TextureView> color_view_;
    ref<TextureView> depth_view_;
    u32 width_{0};
    u32 height_{0};
};

} // namespace woki::rhi
