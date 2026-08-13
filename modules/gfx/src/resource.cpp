#include <woki/gfx/advanced/resource.hpp>

namespace woki::gfx {

void MarkPhysicalResidencyLost(ResidencyRecord& record) noexcept {
    record.resource = ResourceState::RebuildNeeded;
    record.residency = ResidencyState::Lost;
    record.last_used = {};
    record.estimated_bytes = 0;
}

} // namespace woki::gfx
