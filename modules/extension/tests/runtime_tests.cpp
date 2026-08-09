#include <array>
#include <string>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

class FakeBackend final : public woki::ext::RuntimeBackend {
public:
    [[nodiscard]] woki::Result<void> Load(woki::ext::Record& record) override {
        if (fail_load) {
            return woki::Err(woki::ErrorCode::InvalidState, "backend load failed");
        }
        record.tier = woki::ext::RuntimeTier::Wasm;
        loaded = true;
        return woki::Ok();
    }

    [[nodiscard]] woki::Result<void> Initialize(woki::ext::Record&) override {
        if (fail_initialize) {
            return woki::Err(woki::ErrorCode::InvalidState, "backend initialize failed");
        }
        initialized = true;
        return woki::Ok();
    }

    void Tick(woki::ext::Record&, woki::f64 delta_ms) override {
        last_delta_ms = delta_ms;
        ++ticks;
    }

    void DispatchEvent(woki::ext::Record&, woki::u32 event_type, std::span<const woki::u8>) override {
        last_event_type = event_type;
    }

    [[nodiscard]] woki::Result<void> DispatchCommand(woki::ext::Record&, std::string_view command_id, std::span<const woki::u8> payload) override {
        last_command_id = command_id;
        last_command_payload_size = payload.size();
        if (fail_command) {
            return woki::Err(woki::ErrorCode::InvalidState, "backend command failed");
        }
        return woki::Ok();
    }

    void Unload(woki::ext::Record&) override {
        unloaded = true;
        ++unload_calls;
    }

    bool loaded{false};
    bool initialized{false};
    bool unloaded{false};
    bool fail_load{false};
    bool fail_initialize{false};
    bool fail_command{false};
    int unload_calls{0};
    int ticks{0};
    woki::f64 last_delta_ms{0.0};
    woki::u32 last_event_type{0};
    std::string last_command_id;
    std::size_t last_command_payload_size{0};
};

[[nodiscard]] woki::ext::Record MakeRecord() {
    woki::ext::Record record;
    record.id = "woki.hello";
    record.state = woki::ext::State::PermissionChecked;
    return record;
}

} // namespace

TEST_CASE("Extension runtime requires backend") {
    woki::ext::Runtime runtime;
    auto record = MakeRecord();

    auto loaded = runtime.Load(record);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error.contains("backend"));
}

TEST_CASE("Extension runtime drives state machine with backend") {
    FakeBackend backend;
    woki::ext::Runtime runtime(&backend);
    auto record = MakeRecord();

    auto loaded = runtime.Load(record);
    REQUIRE(loaded.has_value());
    REQUIRE(backend.loaded);
    REQUIRE(record.state == woki::ext::State::Loaded);

    auto initialized = runtime.Initialize(record);
    REQUIRE(initialized.has_value());
    REQUIRE(backend.initialized);
    REQUIRE(record.state == woki::ext::State::Active);

    runtime.Tick(record, 16.5);
    REQUIRE(backend.ticks == 1);
    REQUIRE(backend.last_delta_ms == 16.5);

    runtime.DispatchEvent(record, 42, {});
    REQUIRE(backend.last_event_type == 42);

    const std::array<woki::u8, 2> payload{1, 2};
    auto commanded = runtime.DispatchCommand(record, "woki.hello.say", payload);
    REQUIRE(commanded.has_value());
    REQUIRE(backend.last_command_id == "woki.hello.say");
    REQUIRE(backend.last_command_payload_size == payload.size());

    runtime.Unload(record);
    REQUIRE(backend.unloaded);
    REQUIRE(record.state == woki::ext::State::Unloaded);
}

TEST_CASE("Extension runtime rejects commands for inactive records") {
    FakeBackend backend;
    woki::ext::Runtime runtime(&backend);
    auto record = MakeRecord();

    auto commanded = runtime.DispatchCommand(record, "woki.hello.say", {});
    REQUIRE_FALSE(commanded.has_value());
    REQUIRE(commanded.error().Code() == woki::ErrorCode::ValidationInvalidState);
}

TEST_CASE("Extension runtime can swap backends") {
    FakeBackend first;
    woki::ext::Runtime runtime(&first);
    auto first_record = MakeRecord();

    REQUIRE(runtime.Load(first_record).has_value());
    REQUIRE(first.loaded);

    auto second = woki::createScope<FakeBackend>();
    FakeBackend* second_ptr = second.get();
    runtime.SetBackend(std::move(second));

    auto second_record = MakeRecord();
    REQUIRE(runtime.Load(second_record).has_value());
    REQUIRE(second_ptr->loaded);
}

TEST_CASE("Extension runtime records backend load failures") {
    FakeBackend backend;
    backend.fail_load = true;
    woki::ext::Runtime runtime(&backend);
    auto record = MakeRecord();

    auto loaded = runtime.Load(record);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error == "backend load failed");
    REQUIRE_FALSE(backend.loaded);
    REQUIRE(backend.unload_calls == 0);
}

TEST_CASE("Extension runtime records backend initialization failures") {
    FakeBackend backend;
    woki::ext::Runtime runtime(&backend);
    auto record = MakeRecord();
    REQUIRE(runtime.Load(record).has_value());
    backend.fail_initialize = true;

    auto initialized = runtime.Initialize(record);
    REQUIRE_FALSE(initialized.has_value());
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error == "backend initialize failed");
    REQUIRE_FALSE(backend.initialized);
    REQUIRE(backend.unloaded);
    REQUIRE(backend.unload_calls == 1);
    REQUIRE(record.tier == woki::ext::RuntimeTier::None);

    runtime.Unload(record);
    REQUIRE(backend.unload_calls == 1);
}

TEST_CASE("Extension runtime records command failures and stops active dispatch") {
    FakeBackend backend;
    woki::ext::Runtime runtime(&backend);
    auto record = MakeRecord();
    REQUIRE(runtime.Load(record).has_value());
    REQUIRE(runtime.Initialize(record).has_value());
    backend.fail_command = true;

    auto commanded = runtime.DispatchCommand(record, "woki.hello.say", {});
    REQUIRE_FALSE(commanded.has_value());
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error == "backend command failed");
    REQUIRE(backend.unloaded);
    REQUIRE(record.tier == woki::ext::RuntimeTier::None);

    runtime.Tick(record, 1.0);
    runtime.DispatchEvent(record, 42, {});
    REQUIRE(backend.ticks == 0);
    REQUIRE(backend.last_event_type == 0);
}
