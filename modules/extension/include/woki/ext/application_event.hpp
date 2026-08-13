#pragma once

#include <span>
#include <vector>
#include <string_view>

#include <woki/core.hpp>
#include <woki/ext/sdk/events.h>

namespace woki::ext {

enum class ApplicationEventType : u32 {
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) cpp_name = WOKI_EXT_EVENT_##c_name,
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
};

inline constexpr ApplicationEventType kAllApplicationEventTypes[] = {
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) ApplicationEventType::cpp_name,
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
};

[[nodiscard]] constexpr std::span<const ApplicationEventType> AllApplicationEventTypes() noexcept {
    return kAllApplicationEventTypes;
}

[[nodiscard]] std::string_view ToString(ApplicationEventType type) noexcept;
[[nodiscard]] Result<ApplicationEventType> ParseApplicationEventType(std::string_view name);

struct ApplicationEventMetadata final {
    f64 timestamp{};
    u64 sequence{};
    u32 window{};
    u64 device{};
    u16 modifiers{};
    u8 source{};
    bool synthetic{};
};

struct EncodedApplicationEvent final {
    ApplicationEventType type{};
    std::vector<u8> storage;

    [[nodiscard]] std::span<const u8> Payload() const noexcept {
        return storage;
    }
};

class ApplicationEventEncoder final {
public:
    ApplicationEventEncoder(ApplicationEventType type, ApplicationEventMetadata metadata);
    void U8(u8 value);
    void U16(u16 value);
    void U32(u32 value);
    void U64(u64 value);
    void I32(i32 value);
    void F32(f32 value);
    void Pad(std::size_t count);
    void Bytes(std::string_view value);
    void String(std::string_view value);
    [[nodiscard]] EncodedApplicationEvent Finish();

private:
    ApplicationEventType type_{};
    std::vector<u8> storage_;
};

[[nodiscard]] constexpr u32 ExtensionEventId(u32 local_id) noexcept {
    return WOKI_EXT_EVENT_EXTENSION_ID(local_id);
}

} // namespace woki::ext
