#include "woki/ext/limits.hpp"
#include "woki/ext/package.hpp"
#include "woki/ext/wasm/backend.hpp"
#include "woki/ext/wasm/guest_module.hpp"

#if defined(__EMSCRIPTEN__)
#include "woki/ext/wasm/web_engine.hpp"
#elif defined(WOKI_EXTENSION_WITH_WASMTIME)
#include "woki/ext/wasm/wasmtime_engine.hpp"
#endif

#include <string>
#include <filesystem>

namespace woki::ext::wasm {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] Result<void> ValidateWasmHeader(const fs::path& path) {
    return wasm::ValidateWasmMagic(path);
}

[[nodiscard]] Result<void> RequireEngine(const Engine* engine) {
    if (engine == nullptr) {
        return Err(ErrorCode::InvalidState, "Wasm backend has no engine. Provide a Wasmtime/Wasmer engine implementation.");
    }
    return Ok();
}

} // namespace

Backend::Backend(scope<Engine> engine) noexcept
    : engine_(std::move(engine)) {}

scope<Backend> Backend::Create() {
#ifdef __EMSCRIPTEN__
    return createScope<Backend>(createScope<WebEngine>());
#elif defined(WOKI_EXTENSION_WITH_WASMTIME)
    return createScope<Backend>(createScope<WasmtimeEngine>());
#else
    return createScope<Backend>(nullptr);
#endif
}

Result<void> Backend::Load(Record& record) {
    auto engine = RequireEngine(engine_.get());
    if (!engine) {
        return Err(engine.error());
    }

    auto layout = ValidatePackageLayout(record.package);
    if (!layout) {
        return Err(layout.error());
    }

    auto header = ValidateWasmHeader(record.package.wasm);
    if (!header) {
        return Err(header.error());
    }

    host::HostApi host(record);
    auto loaded = engine_->Load(record, host);
    if (!loaded) {
        record.tier = RuntimeTier::None;
        return Err(loaded.error());
    }

    auto api_version = engine_->ApiVersion(record);
    if (!api_version) {
        engine_->Discard(record);
        record.tier = RuntimeTier::None;
        return Err(api_version.error());
    }
    if (*api_version != record.manifest.api_version) {
        engine_->Discard(record);
        record.tier = RuntimeTier::None;
        return Err(ErrorCode::ValidationInvalidState, "Extension apiVersion mismatch. Manifest declares " + std::to_string(record.manifest.api_version) + ", wasm exports " + std::to_string(*api_version) + ".");
    }

#if defined(__EMSCRIPTEN__)
    record.tier = RuntimeTier::Web;
#else
    record.tier = RuntimeTier::Wasm;
#endif
    return Ok();
}

Result<void> Backend::Initialize(Record& record) {
    auto engine = RequireEngine(engine_.get());
    if (!engine) {
        return Err(engine.error());
    }

    auto result = engine_->Init(record);
    if (!result) {
        engine_->Discard(record);
        record.tier = RuntimeTier::None;
        return Err(result.error());
    }
    if (*result != 0) {
        engine_->Discard(record);
        record.tier = RuntimeTier::None;
        return Err(ErrorCode::ValidationInvalidState, "Extension ext_init returned failure code " + std::to_string(*result) + ".");
    }

    return Ok();
}

void Backend::Tick(Record& record, f64 delta_ms) {
    if (engine_ == nullptr) {
        return;
    }
    auto ticked = engine_->Tick(record, delta_ms);
    if (!ticked) {
        record.state = State::Failed;
        record.error = std::string(ticked.error().Message());
    }
}

void Backend::DispatchEvent(Record& record, u32 event_type, std::span<const u8> payload) {
    if (engine_ == nullptr) {
        return;
    }
    if (payload.size() > limits::kMaxEventBytes) {
        return;
    }
    auto dispatched = engine_->Event(record, event_type, payload);
    if (!dispatched) {
        record.state = State::Failed;
        record.error = std::string(dispatched.error().Message());
    }
}

Result<void> Backend::DispatchCommand(Record& record, std::string_view command_id, std::span<const u8> payload) {
    if (engine_ == nullptr) {
        return Err(ErrorCode::InvalidState, "Wasm backend has no engine. Cannot dispatch extension command.");
    }
    if (command_id.empty() || command_id.size() > limits::kMaxPathBytes || payload.size() > limits::kMaxEventBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension command id or payload exceeds the ABI size limit.");
    }

    auto result = engine_->Command(record, command_id, payload);
    if (!result) {
        return Err(result.error());
    }
    if (*result != 0) {
        return Err(ErrorCode::ValidationInvalidState, "Extension command '" + std::string(command_id) + "' returned failure code " + std::to_string(*result) + ".");
    }
    return Ok();
}

void Backend::Unload(Record& record) {
    if (engine_ == nullptr) {
        return;
    }
    engine_->Unload(record);
    record.tier = RuntimeTier::None;
}

} // namespace woki::ext::wasm
