#include <algorithm>

#include "woki/ext/command.hpp"
#include "woki/ext/runtime.hpp"
#include "woki/ext/registry.hpp"

namespace woki::ext {

void CommandRegistry::Clear() noexcept {
    records_.clear();
}

void CommandRegistry::Register(std::string_view extension_id, const std::vector<CommandContribution>& commands) {
    for (const CommandContribution& command : commands) {
        records_.push_back(CommandRecord{
            .extension_id = std::string(extension_id),
            .command = command,
        });
    }
}

const std::vector<CommandRecord>& CommandRegistry::Records() const noexcept {
    return records_;
}

const CommandRecord* CommandRegistry::Find(std::string_view command_id) const noexcept {
    const auto it = std::ranges::find_if(records_, [command_id](const CommandRecord& record) { return record.command.id == command_id; });
    if (it == records_.end()) {
        return nullptr;
    }
    return &*it;
}

Result<void> CommandDispatcher::Execute(const CommandRegistry& registry, Runtime& runtime, std::span<Record> records, std::string_view command_id, std::span<const u8> payload) const {
    const CommandRecord* command = registry.Find(command_id);
    if (command == nullptr) {
        return Err(ErrorCode::FileNotFound, "Extension command '" + std::string(command_id) + "' is not registered. Run Scan() first and check manifest.yaml contributes.");
    }

    const auto record_it = std::ranges::find(records, command->extension_id, &Record::id);
    if (record_it == records.end()) {
        return Err(ErrorCode::FileNotFound, "Extension '" + command->extension_id + "' for command '" + std::string(command_id) + "' is not registered.");
    }

    if (record_it->state == State::PermissionChecked) {
        auto loaded = runtime.Load(*record_it);
        if (!loaded) {
            return Err(loaded.error());
        }
        auto initialized = runtime.Initialize(*record_it);
        if (!initialized) {
            return Err(initialized.error());
        }
    }

    return runtime.DispatchCommand(*record_it, command->command.id, payload);
}

} // namespace woki::ext
