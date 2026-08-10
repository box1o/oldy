#pragma once

#include "manifest.hpp"

namespace woki::ext {

class CapabilityPolicy {
public:
    virtual ~CapabilityPolicy() = default;
    [[nodiscard]] virtual Result<EffectiveCapabilities> Grant(const Manifest& manifest) const = 0;
};

class PermissiveCapabilityPolicy final : public CapabilityPolicy {
public:
    [[nodiscard]] Result<EffectiveCapabilities> Grant(const Manifest& manifest) const override;
};

} // namespace woki::ext
