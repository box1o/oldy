#pragma once

#include <functional>
#include <string>
#include <vector>

#include "device.hpp"

namespace woki::rhi {

struct ConformanceReport final {
    SubmissionTicket submission;
    SubmissionEpoch completed;
    bool buffer_copy_write_map{};
    bool texture_upload_copy{};
    bool render_commands{};
    bool compute_commands{};
    bool readback{};
    std::vector<std::string> notes;
};

// `pump` must process backend callbacks. Returning false cancels the bounded wait.
[[nodiscard]] Result<ConformanceReport> RunConformance(Device& device, const std::function<bool()>& pump = {});

} // namespace woki::rhi
