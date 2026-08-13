#include <catch2/catch_test_macros.hpp>

#include "rhi_test_fixture.hpp"

using namespace woki;
using namespace woki::rhi;
using namespace woki::rhi::test;

TEST_CASE("Shared RHI conformance runner passes against NullRHI without a display") {
    NullRhiDescriptor descriptor;
    descriptor.complete_submissions_immediately = false;
    NullContext context(descriptor);
    u32 pump_count = 0;

    auto result = RunConformance(*context.device, [&] {
        ++pump_count;
        return true;
    });

    REQUIRE(result);
    CHECK(result->submission.IsValid());
    CHECK(result->completed.HasReached(result->submission));
    CHECK(result->buffer_copy_write_map);
    CHECK(result->texture_upload_copy);
    CHECK(result->render_commands);
    CHECK(result->compute_commands);
    CHECK(result->readback);
    CHECK(pump_count > 0);
    CHECK(LogContains(*context.device, "render.draw 3 1"));
    CHECK(LogContains(*context.device, "compute.dispatch 1 1 1"));
}
