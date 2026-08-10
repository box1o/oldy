#pragma once

// Host implementation detail. This header is not installed.

#include <span>
#include <vector>
#include <string_view>

#include <woki/core.hpp>

#include "../command.hpp"

namespace woki::ext {

class CommandIndex final {
public:
    void Clear() noexcept;
    [[nodiscard]] Result<void> Add(std::string_view extension_id, const std::vector<CommandContribution>& commands);
    [[nodiscard]] std::span<const CommandRecord> Records() const noexcept;
    [[nodiscard]] const CommandRecord* Find(std::string_view command_id) const noexcept;

private:
    std::vector<CommandRecord> records_;
};

} // namespace woki::ext
