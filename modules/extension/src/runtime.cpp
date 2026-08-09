#include <string>

#include "woki/ext/runtime.hpp"

namespace woki::ext {

namespace {

[[nodiscard]] Result<void> RequireBackend(RuntimeBackend* backend) {
    if (backend == nullptr) {
        return Err(ErrorCode::InvalidState, "Extension runtime backend is not configured. Add the Wasmtime backend before loading "
                                            "extensions.");
    }
    return Ok();
}

void MarkFailed(Record& record, const Error& error) {
    record.state = State::Failed;
    record.error = std::string(error.Message());
}

} // namespace

Runtime::Runtime(RuntimeBackend* backend) noexcept
    : backend_(backend) {}

Runtime::Runtime(scope<RuntimeBackend> backend) noexcept {
    SetBackend(std::move(backend));
}

void Runtime::SetBackend(RuntimeBackend* backend) noexcept {
    owned_backend_.reset();
    backend_ = backend;
}

void Runtime::SetBackend(scope<RuntimeBackend> backend) noexcept {
    owned_backend_ = std::move(backend);
    backend_ = owned_backend_.get();
}

RuntimeBackend* Runtime::Backend() noexcept {
    return backend_;
}

const RuntimeBackend* Runtime::Backend() const noexcept {
    return backend_;
}

Result<void> Runtime::Load(Record& record) {
    if (record.state != State::PermissionChecked) {
        return Err(ErrorCode::ValidationInvalidState, "Extension can only be loaded after manifest and permissions are validated.");
    }

    auto backend = RequireBackend(backend_);
    if (!backend) {
        MarkFailed(record, backend.error());
        return Err(backend.error());
    }

    auto loaded = backend_->Load(record);
    if (!loaded) {
        MarkFailed(record, loaded.error());
        return Err(loaded.error());
    }

    record.state = State::Loaded;
    return Ok();
}

Result<void> Runtime::Initialize(Record& record) {
    if (record.state != State::Loaded) {
        return Err(ErrorCode::ValidationInvalidState, "Extension can only be initialized after it is loaded.");
    }

    auto backend = RequireBackend(backend_);
    if (!backend) {
        MarkFailed(record, backend.error());
        return Err(backend.error());
    }

    auto initialized = backend_->Initialize(record);
    if (!initialized) {
        MarkFailed(record, initialized.error());
        return Err(initialized.error());
    }

    record.state = State::Initialized;
    record.state = State::Active;
    return Ok();
}

void Runtime::Tick(Record& record, f64 delta_ms) {
    if (record.state != State::Active || backend_ == nullptr) {
        return;
    }
    backend_->Tick(record, delta_ms);
}

void Runtime::DispatchEvent(Record& record, u32 event_type, std::span<const u8> payload) {
    if (record.state != State::Active || backend_ == nullptr) {
        return;
    }
    backend_->DispatchEvent(record, event_type, payload);
}

Result<void> Runtime::DispatchCommand(Record& record, std::string_view command_id, std::span<const u8> payload) {
    if (record.state != State::Active) {
        return Err(ErrorCode::ValidationInvalidState, "Extension command can only run while the extension is active.");
    }

    auto backend = RequireBackend(backend_);
    if (!backend) {
        MarkFailed(record, backend.error());
        return Err(backend.error());
    }

    auto dispatched = backend_->DispatchCommand(record, command_id, payload);
    if (!dispatched) {
        MarkFailed(record, dispatched.error());
        return Err(dispatched.error());
    }
    return Ok();
}

void Runtime::Unload(Record& record) {
    if (record.state != State::Active || backend_ == nullptr) {
        return;
    }

    record.state = State::Unloading;
    backend_->Unload(record);
    record.state = State::Unloaded;
}

} // namespace woki::ext
