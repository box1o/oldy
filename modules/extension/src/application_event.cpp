#include <bit>
#include <string>
#include <algorithm>

#include "woki/ext/application_event.hpp"

namespace woki::ext {
namespace {
struct ApplicationEventName {
    ApplicationEventType type;
    std::string_view name;
};

constexpr ApplicationEventName kNames[] = {
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) {ApplicationEventType::cpp_name, #name},
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
};

void StoreU32(std::vector<u8>& out, std::size_t offset, u32 value) {
    out[offset] = static_cast<u8>(value);
    out[offset + 1] = static_cast<u8>(value >> 8u);
    out[offset + 2] = static_cast<u8>(value >> 16u);
    out[offset + 3] = static_cast<u8>(value >> 24u);
}
} // namespace

std::string_view ToString(ApplicationEventType type) noexcept {
    switch (type) {
        case ApplicationEventType::WindowClosed:
            return "window.closed";
        case ApplicationEventType::WindowResized:
            return "window.resized";
        case ApplicationEventType::WindowFocused:
            return "window.focused";
        case ApplicationEventType::WindowLostFocus:
            return "window.lost-focus";
        case ApplicationEventType::WindowMoved:
            return "window.moved";
        case ApplicationEventType::WindowMinimized:
            return "window.minimized";
        case ApplicationEventType::WindowMaximized:
            return "window.maximized";
        case ApplicationEventType::WindowRestored:
            return "window.restored";
        case ApplicationEventType::KeyPressed:
            return "key.pressed";
        case ApplicationEventType::KeyReleased:
            return "key.released";
        case ApplicationEventType::WindowScaleChanged:
            return "window.scale-changed";
        case ApplicationEventType::ViewportResized:
            return "viewport.resized";
        case ApplicationEventType::AppShutdown:
            return "app.shutdown";
        case ApplicationEventType::AppSuspend:
            return "app.suspend";
        case ApplicationEventType::AppResume:
            return "app.resume";
        default:
            break;
    }
    const auto found = std::ranges::find(kNames, type, &ApplicationEventName::type);
    return found == std::end(kNames) ? "unknown" : found->name;
}

Result<ApplicationEventType> ParseApplicationEventType(std::string_view name) {
    for (const ApplicationEventType type : AllApplicationEventTypes())
        if (ToString(type) == name)
            return Ok(type);
    return Err(ErrorCode::ValidationInvalidState, "Unknown application event '" + std::string(name) + "'.");
}

ApplicationEventEncoder::ApplicationEventEncoder(ApplicationEventType type, ApplicationEventMetadata metadata)
    : type_(type) {
    U16(WOKI_EXT_EVENT_ABI_VERSION);
    U16(WOKI_EXT_EVENT_METADATA_SIZE);
    U32(0);
    U64(std::bit_cast<u64>(metadata.timestamp));
    U64(metadata.sequence);
    U32(metadata.window);
    U64(metadata.device);
    U16(metadata.modifiers);
    U8(metadata.source);
    U8(metadata.synthetic ? 1u : 0u);
}

void ApplicationEventEncoder::U8(u8 value) {
    storage_.push_back(value);
}

void ApplicationEventEncoder::U16(u16 value) {
    U8(static_cast<u8>(value));
    U8(static_cast<u8>(value >> 8u));
}

void ApplicationEventEncoder::U32(u32 value) {
    for (u32 shift = 0; shift < 32; shift += 8)
        U8(static_cast<u8>(value >> shift));
}

void ApplicationEventEncoder::U64(u64 value) {
    for (u32 shift = 0; shift < 64; shift += 8)
        U8(static_cast<u8>(value >> shift));
}

void ApplicationEventEncoder::I32(i32 value) {
    U32(static_cast<u32>(value));
}

void ApplicationEventEncoder::F32(f32 value) {
    U32(std::bit_cast<u32>(value));
}

void ApplicationEventEncoder::Pad(std::size_t count) {
    storage_.insert(storage_.end(), count, 0);
}

void ApplicationEventEncoder::Bytes(std::string_view value) {
    storage_.insert(storage_.end(), value.begin(), value.end());
}

void ApplicationEventEncoder::String(std::string_view value) {
    U32(static_cast<u32>(value.size()));
    Bytes(value);
}

EncodedApplicationEvent ApplicationEventEncoder::Finish() {
    StoreU32(storage_, 4, static_cast<u32>(storage_.size()));
    return {.type = type_, .storage = std::move(storage_)};
}
} // namespace woki::ext
