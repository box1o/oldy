#pragma once

#include <string>

#include "forward.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

struct SwapchainDesc final {
    u32 width{0};
    u32 height{0};
    TextureFormat format{TextureFormat::BGRA8Unorm};
    TextureFormat depth_format{TextureFormat::Depth24PlusStencil8};
    TextureUsage usage{TextureUsage::RenderAttachment};
    PresentMode present_mode{PresentMode::Fifo};
    CompositeAlphaMode alpha_mode{CompositeAlphaMode::Auto};
    bool enable_depth{true};
    std::string label{"Swapchain"};
};

} // namespace woki::rhi
