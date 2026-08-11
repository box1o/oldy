#include <algorithm>

#include "woki/ext/runtime.hpp"
#include "woki/ext/internal/event_service.hpp"

namespace woki::ext {

namespace {
[[nodiscard]] bool IsNonfatalCommandError(ErrorCode code) noexcept {
    return code == ErrorCode::FileAccessDenied || code == ErrorCode::ValidationOutOfRange || code == ErrorCode::FileNotFound || code == ErrorCode::InvalidArgument;
}
} // namespace

struct Runtime::Session {
    std::string extension_id;
    scope<RuntimeInstance> instance;
    std::shared_ptr<host::EventSession> events;
    EffectiveCapabilities grants;
};

Runtime::Runtime(scope<RuntimeEngine> engine) noexcept
    : engine_(std::move(engine)) {}

Runtime::~Runtime() {
    UnloadAll();
}

void Runtime::SetEngine(scope<RuntimeEngine> engine) noexcept {
    UnloadAll();
    engine_ = std::move(engine);
}

void Runtime::SetEventService(std::shared_ptr<host::EventService> service) noexcept {
    UnloadAll();
    event_service_ = std::move(service);
}

Result<void> Runtime::Load(const ExtensionPackage& package, EffectiveCapabilities grants) {
    if (IsActive(package.Id()))
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + package.Id() + "' already has an active runtime instance.");
    std::erase_if(statuses_, [&package](const ExtensionStatus& status) { return status.extension_id == package.Id(); });
    if (engine_ == nullptr) {
        const Error error(ErrorCode::InvalidState, "Extension runtime engine is not configured.");
        statuses_.push_back({package.Id(), ExtensionState::Failed, error.Code(), std::string(error.Message())});
        return Err(error);
    }
    if (auto valid = ValidatePackageLayout(package.Layout()); !valid) {
        statuses_.push_back({package.Id(), ExtensionState::Failed, valid.error().Code(), std::string(valid.error().Message())});
        return Err(valid.error());
    }

    const std::size_t event_checkpoint = event_service_ == nullptr ? 0 : event_service_->Checkpoint();
    const auto discard_activation_events = [this, event_checkpoint] {
        if (event_service_ != nullptr)
            event_service_->DiscardAfter(event_checkpoint);
    };
    auto events = std::make_shared<host::EventSession>();
    host::Context context{package.Id(), grants.permissions, package.Layout().data_root, package.Layout().config_root, package.Layout().cache_root, events, event_service_};
    auto instance = engine_->Create(package, host::HostApi(std::move(context)));
    if (!instance) {
        discard_activation_events();
        statuses_.push_back({package.Id(), ExtensionState::Failed, instance.error().Code(), std::string(instance.error().Message())});
        return Err(instance.error());
    }
    if (*instance == nullptr) {
        discard_activation_events();
        const Error error(ErrorCode::InvalidState, "Extension runtime engine returned a null instance.");
        statuses_.push_back({package.Id(), ExtensionState::Failed, error.Code(), std::string(error.Message())});
        return Err(error);
    }
    auto initialized = (*instance)->Initialize();
    if (!initialized) {
        (*instance)->Unload();
        discard_activation_events();
        statuses_.push_back({package.Id(), ExtensionState::Failed, initialized.error().Code(), std::string(initialized.error().Message())});
        return Err(initialized.error());
    }
    sessions_.push_back(createScope<Session>(Session{package.Id(), std::move(*instance), std::move(events), std::move(grants)}));
    statuses_.push_back({package.Id(), ExtensionState::Active, ErrorCode::Success, {}});
    return Ok();
}

Result<void> Runtime::Load(const ExtensionPackage& package) {
    return Load(package, EffectiveCapabilities{package.GetManifest().requested_capabilities.permissions});
}

void Runtime::RecordFailure(std::string_view extension_id, const Error& error) {
    std::erase_if(statuses_, [extension_id](const ExtensionStatus& status) { return status.extension_id == extension_id; });
    statuses_.push_back({std::string(extension_id), ExtensionState::Failed, error.Code(), std::string(error.Message())});
}

void Runtime::Tick(f64 delta_ms) {
    for (std::size_t i = 0; i < sessions_.size();) {
        auto result = sessions_[i]->instance->Tick(delta_ms);
        if (result) {
            ++i;
            continue;
        }
        const std::string id = sessions_[i]->extension_id;
        sessions_[i]->instance->Unload();
        sessions_.erase(sessions_.begin() + static_cast<std::ptrdiff_t>(i));
        if (auto status = std::ranges::find(statuses_, id, &ExtensionStatus::extension_id); status != statuses_.end()) {
            status->state = ExtensionState::Failed;
            status->error_code = result.error().Code();
            status->error = std::string(result.error().Message());
        }
    }
}

void Runtime::Tick(std::string_view extension_id, f64 delta_ms) {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    if (it == sessions_.end())
        return;
    auto result = (*it)->instance->Tick(delta_ms);
    if (result)
        return;
    const std::string id = (*it)->extension_id;
    (*it)->instance->Unload();
    sessions_.erase(it);
    if (auto status = std::ranges::find(statuses_, id, &ExtensionStatus::extension_id); status != statuses_.end()) {
        status->state = ExtensionState::Failed;
        status->error_code = result.error().Code();
        status->error = std::string(result.error().Message());
    }
}

void Runtime::DispatchEvent(std::string_view extension_id, u32 event_type, std::span<const u8> payload) {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    if (it == sessions_.end())
        return;
    auto result = (*it)->instance->DispatchEvent(event_type, payload);
    if (result)
        return;
    const std::string id = (*it)->extension_id;
    (*it)->instance->Unload();
    sessions_.erase(it);
    if (auto status = std::ranges::find(statuses_, id, &ExtensionStatus::extension_id); status != statuses_.end()) {
        status->state = ExtensionState::Failed;
        status->error_code = result.error().Code();
        status->error = std::string(result.error().Message());
    }
}

void Runtime::DispatchNamedEvent(std::string_view extension_id, std::string_view topic, std::span<const u8> payload) {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    if (it == sessions_.end())
        return;
    auto result = (*it)->instance->DispatchNamedEvent(topic, payload);
    if (result)
        return;
    const std::string id = (*it)->extension_id;
    (*it)->instance->Unload();
    sessions_.erase(it);
    if (auto status = std::ranges::find(statuses_, id, &ExtensionStatus::extension_id); status != statuses_.end()) {
        status->state = ExtensionState::Failed;
        status->error_code = result.error().Code();
        status->error = std::string(result.error().Message());
    }
}

Result<void> Runtime::DispatchCommand(std::string_view extension_id, std::string_view command_id, std::span<const u8> payload) {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    if (it == sessions_.end())
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + std::string(extension_id) + "' is not active.");
    auto result = (*it)->instance->DispatchCommand(command_id, payload);
    if (!result && !IsNonfatalCommandError(result.error().Code())) {
        const std::string id = (*it)->extension_id;
        (*it)->instance->Unload();
        sessions_.erase(it);
        if (auto status = std::ranges::find(statuses_, id, &ExtensionStatus::extension_id); status != statuses_.end()) {
            status->state = ExtensionState::Failed;
            status->error_code = result.error().Code();
            status->error = std::string(result.error().Message());
        }
    }
    return result;
}

void Runtime::Unload(std::string_view extension_id) noexcept {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    if (it != sessions_.end()) {
        (*it)->instance->Unload();
        sessions_.erase(it);
    }
    std::erase_if(statuses_, [extension_id](const ExtensionStatus& status) { return status.extension_id == extension_id; });
}

void Runtime::UnloadAll() noexcept {
    for (auto& session : sessions_)
        session->instance->Unload();
    sessions_.clear();
    statuses_.clear();
}

bool Runtime::IsActive(std::string_view extension_id) const noexcept {
    return std::ranges::any_of(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
}

bool Runtime::IsSubscribed(std::string_view extension_id, u32 event_type) const noexcept {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    return it != sessions_.end() && (*it)->events->IsSubscribed(event_type);
}

bool Runtime::IsSubscribed(std::string_view extension_id, std::string_view topic) const noexcept {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    return it != sessions_.end() && (*it)->events->IsSubscribed(topic);
}

bool Runtime::HasGrant(std::string_view extension_id, Permission permission) const noexcept {
    const auto it = std::ranges::find_if(sessions_, [extension_id](const auto& session) { return session->extension_id == extension_id; });
    return it != sessions_.end() && HasPermission((*it)->grants, permission);
}

std::span<const ExtensionStatus> Runtime::Statuses() const noexcept {
    return statuses_;
}

} // namespace woki::ext
