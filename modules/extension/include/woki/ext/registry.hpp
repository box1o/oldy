#pragma once

// Host implementation detail. This header is not installed.

#include <span>
#include <string>
#include <vector>
#include <filesystem>

#include "package.hpp"

namespace woki::ext {

class Registry final {
public:
    void Clear() noexcept;
    [[nodiscard]] Result<void> Add(ExtensionPackage package);
    void AddFailure(DiscoveryFailure failure);
    [[nodiscard]] Result<void> Scan(const Roots& roots);
    [[nodiscard]] Result<void> ScanSource(const std::filesystem::path& source_root, const Roots& roots);
    [[nodiscard]] std::span<const ExtensionPackage> Packages() const noexcept;
    [[nodiscard]] std::span<const DiscoveryFailure> Failures() const noexcept;
    [[nodiscard]] const ExtensionPackage* Find(std::string_view id) const noexcept;

private:
    std::vector<ExtensionPackage> packages_;
    std::vector<DiscoveryFailure> failures_;
};

} // namespace woki::ext
