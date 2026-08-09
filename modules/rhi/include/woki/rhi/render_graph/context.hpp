#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>

#include <woki/core.hpp>
#include <woki/rhi/forward.hpp>

#include "resources.hpp"

namespace woki::rhi {

class CommandEncoder;
class Device;
class RenderPassEncoder;

class RenderPassContext final {
public:
    [[nodiscard]] RenderPassEncoder& encoder();
    [[nodiscard]] Device& device() noexcept;
    [[nodiscard]] const ref<Device>& device_ref() const noexcept;

    [[nodiscard]] TextureView& color(u32 slot);
    [[nodiscard]] TextureView& depth();
    [[nodiscard]] bool has_depth() const noexcept;

    [[nodiscard]] TextureView& sample(u32 slot);
    [[nodiscard]] u32 sample_count() const noexcept;

    [[nodiscard]] u32 width() const noexcept {
        return width_;
    }

    [[nodiscard]] u32 height() const noexcept {
        return height_;
    }

    template <typename T>
    [[nodiscard]] T& data() {
        WOKI_ASSERT(user_data_ != nullptr);
        return *static_cast<T*>(user_data_.get());
    }

    [[nodiscard]] BindGroup* GetOrCreateBindGroup(std::string_view key, std::function<scope<BindGroup>()> factory);

private:
    friend class RenderGraph;
    friend class RenderGraphFrame;

    RenderPassEncoder* pass_{nullptr};
    ref<Device> device_{};
    ref<void> user_data_{};
    u32 width_{0};
    u32 height_{0};

    std::vector<TextureView*> colors_{};
    TextureView* depth_{nullptr};
    std::vector<TextureView*> samples_{};

    std::unordered_map<std::string, scope<BindGroup>> bind_group_cache_{};
};

class CopyPassContext final {
public:
    [[nodiscard]] CommandEncoder& encoder();
    [[nodiscard]] Device& device() noexcept;
    [[nodiscard]] const ref<Device>& device_ref() const noexcept;

    [[nodiscard]] Texture& src(u32 index);
    [[nodiscard]] Texture& dst(u32 index);

    [[nodiscard]] u32 copy_count() const noexcept {
        return static_cast<u32>(sources_.size());
    }

    [[nodiscard]] Result<void> CopyAll();

    template <typename T>
    [[nodiscard]] T& data() {
        WOKI_ASSERT(user_data_ != nullptr);
        return *static_cast<T*>(user_data_.get());
    }

private:
    friend class RenderGraph;
    friend class RenderGraphFrame;

    CommandEncoder* encoder_{nullptr};
    ref<Device> device_{};
    ref<void> user_data_{};
    u32 width_{0};
    u32 height_{0};

    std::vector<Texture*> sources_{};
    std::vector<Texture*> destinations_{};
};

} // namespace woki::rhi
