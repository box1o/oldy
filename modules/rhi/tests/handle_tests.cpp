#include <catch2/catch_test_macros.hpp>

#include "detail/handle.hpp"

namespace {

struct FakeHandle final {
    int references{1};
    int releases{0};
};

void AddRef(FakeHandle* handle) {
    ++handle->references;
}

void Release(FakeHandle* handle) {
    --handle->references;
    ++handle->releases;
}

using Handle = woki::rhi::wgpu::detail::GpuHandle<FakeHandle*, AddRef, Release>;

} // namespace

TEST_CASE("GpuHandle adopts and releases a native handle") {
    FakeHandle native{};

    {
        Handle handle(&native);
        REQUIRE(handle.get() == &native);
        REQUIRE(native.references == 1);
    }

    REQUIRE(native.references == 0);
    REQUIRE(native.releases == 1);
}

TEST_CASE("GpuHandle copies with native reference counting") {
    FakeHandle native{};

    {
        Handle first(&native);
        Handle second(first);

        REQUIRE(first.get() == &native);
        REQUIRE(second.get() == &native);
        REQUIRE(native.references == 2);
    }

    REQUIRE(native.references == 0);
    REQUIRE(native.releases == 2);
}

TEST_CASE("GpuHandle moves without changing the reference count") {
    FakeHandle native{};

    {
        Handle first(&native);
        Handle second(std::move(first));

        REQUIRE(second.get() == &native);
        REQUIRE(native.references == 1);
    }

    REQUIRE(native.references == 0);
    REQUIRE(native.releases == 1);
}

TEST_CASE("GpuHandle retains borrowed native handles") {
    FakeHandle native{};

    {
        auto handle = Handle::Retain(&native);
        REQUIRE(handle.get() == &native);
        REQUIRE(native.references == 2);
    }

    REQUIRE(native.references == 1);
    REQUIRE(native.releases == 1);
}

TEST_CASE("GpuHandle copy assignment releases the previous handle") {
    FakeHandle first_native{};
    FakeHandle second_native{};

    {
        Handle first(&first_native);
        Handle second(&second_native);

        second = first;

        REQUIRE(first_native.references == 2);
        REQUIRE(second_native.references == 0);
        REQUIRE(second_native.releases == 1);
    }

    REQUIRE(first_native.references == 0);
    REQUIRE(first_native.releases == 2);
}
