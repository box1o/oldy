#include "woki/ext/policy.hpp"

namespace woki::ext {

Result<EffectiveCapabilities> PermissiveCapabilityPolicy::Grant(const Manifest& manifest) const {
    return Ok(EffectiveCapabilities{manifest.requested_capabilities.permissions});
}

} // namespace woki::ext
