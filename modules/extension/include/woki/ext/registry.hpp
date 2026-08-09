#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <string>
#include <vector>
#include <filesystem>

#include "state.hpp"
#include "package.hpp"

namespace woki::ext {

struct Record {
    std::string id;
    Manifest manifest;
    PackageLayout package;
    State state{State::Discovered};
    RuntimeTier tier{RuntimeTier::None};
    std::string error;
};

[[nodiscard]] Result<Roots> DefaultRoots();

class Registry final {
public:
    void SetRoots(Roots roots);

    [[nodiscard]] Result<void> Scan();
    [[nodiscard]] Result<void> ScanSource(const std::filesystem::path& source_root);
    [[nodiscard]] const std::vector<Record>& Records() const noexcept;
    [[nodiscard]] std::vector<Record>& Records() noexcept;

private:
    Roots roots_;
    std::vector<Record> records_;
};

} // namespace woki::ext
