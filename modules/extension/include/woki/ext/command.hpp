#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <span>
#include <string>
#include <vector>
#include <string_view>

#include <woki/core.hpp>

namespace woki::ext {

struct Record;
class Runtime;

struct CommandContribution {
    std::string id;
    std::string title;
    std::string category;
};

struct CommandRecord {
    std::string extension_id;
    CommandContribution command;
};

class CommandRegistry final {
public:
    void Clear() noexcept;
    void Register(std::string_view extension_id, const std::vector<CommandContribution>& commands);

    [[nodiscard]] const std::vector<CommandRecord>& Records() const noexcept;
    [[nodiscard]] const CommandRecord* Find(std::string_view command_id) const noexcept;

private:
    std::vector<CommandRecord> records_;
};

class CommandDispatcher final {
public:
    [[nodiscard]] Result<void> Execute(const CommandRegistry& registry, Runtime& runtime, std::span<Record> records, std::string_view command_id, std::span<const u8> payload) const;
};

} // namespace woki::ext
