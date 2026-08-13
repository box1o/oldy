#pragma once

#include <variant>
#include <compare>
#include <cstddef>
#include <string>
#include <vector>

#include "compute.hpp"
#include "presentation.hpp"

namespace woki::gfx {

class ReadbackTicket final {
public:
    constexpr ReadbackTicket() noexcept = default;

    explicit constexpr ReadbackTicket(const u64 value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const ReadbackTicket&, const ReadbackTicket&) = default;

private:
    u64 value_{};
};

enum class ReadbackState : u8 {
    Queued,
    Submitted,
    Mapping,
    Ready,
    Consumed,
    Failed,
    DeviceLost,
    Cancelled,
};

struct TextureSubresource final {
    u32 mip_level{};
    u32 array_layer{};
    u32 x{};
    u32 y{};
    u32 width{};
    u32 height{};
};

struct BufferReadback final {
    BufferHandle buffer;
    u64 offset{};
    u64 size{};
};

struct TextureReadback final {
    GpuTextureHandle texture;
    TextureSubresource subresource;
};

struct OffscreenReadback final {
    OffscreenTargetHandle target;
    TextureSubresource subresource;
};

using LogicalReadbackSource = std::variant<BufferReadback, TextureReadback, OffscreenReadback>;

struct ReadbackImage final {
    u32 width{};
    u32 height{};
    u32 row_bytes{};
    PixelFormat format{PixelFormat::RGBA8Unorm};
    std::vector<std::byte> pixels;

    [[nodiscard]] Result<ReadbackImage> ToRgba8() const;
};

using ReadbackResult = std::variant<std::vector<std::byte>, ReadbackImage>;

class ReadbackLease final {
public:
    ReadbackLease() = default;

    explicit ReadbackLease(ref<const ReadbackResult> result)
        : result_(std::move(result)) {}

    [[nodiscard]] const ReadbackResult* Get() const noexcept {
        return result_.get();
    }

    [[nodiscard]] const ReadbackResult& operator*() const noexcept {
        return *result_;
    }

    [[nodiscard]] const ReadbackResult* operator->() const noexcept {
        return result_.get();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return result_ != nullptr;
    }

private:
    ref<const ReadbackResult> result_;
};

class ReadbackService final {
public:
    ReadbackService();
    ~ReadbackService();
    [[nodiscard]] Result<ReadbackTicket> Request(LogicalReadbackSource source);
    [[nodiscard]] ReadbackState Poll(ReadbackTicket ticket) const noexcept;
    [[nodiscard]] Result<ReadbackLease> TryMap(ReadbackTicket ticket) const;
    [[nodiscard]] Result<u32> TryMapPickingU32(ReadbackTicket ticket) const;
    [[nodiscard]] Result<void> Release(ReadbackTicket ticket);
    [[nodiscard]] Result<void> Cancel(ReadbackTicket ticket);
    [[nodiscard]] std::string Error(ReadbackTicket ticket) const;

private:
    friend class RenderRuntime;
    friend struct RuntimeFacadeAccess;
    struct Impl;
    scope<Impl> impl_;
};

} // namespace woki::gfx
