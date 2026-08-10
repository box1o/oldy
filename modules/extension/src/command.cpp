#include <algorithm>

#include "woki/ext/internal/command_index.hpp"

namespace woki::ext {

void CommandIndex::Clear() noexcept {
    records_.clear();
}

Result<void> CommandIndex::Add(std::string_view extension_id, const std::vector<CommandContribution>& commands) {
    for (const CommandContribution& command : commands) {
        if (Find(command.id) != nullptr) {
            return Err(ErrorCode::ValidationInvalidState, "Duplicate extension command id '" + command.id + "'.");
        }
        if (std::ranges::count(commands, command.id, &CommandContribution::id) != 1) {
            return Err(ErrorCode::ValidationInvalidState, "Duplicate extension command id '" + command.id + "'.");
        }
    }
    for (const CommandContribution& command : commands) {
        records_.push_back(CommandRecord{
            .extension_id = std::string(extension_id),
            .command = command,
        });
    }
    return Ok();
}

std::span<const CommandRecord> CommandIndex::Records() const noexcept {
    return records_;
}

const CommandRecord* CommandIndex::Find(std::string_view command_id) const noexcept {
    const auto it = std::ranges::find_if(records_, [command_id](const CommandRecord& record) { return record.command.id == command_id; });
    if (it == records_.end()) {
        return nullptr;
    }
    return &*it;
}

} // namespace woki::ext
