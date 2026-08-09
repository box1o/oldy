#include <array>
#include <string>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

namespace fs = std::filesystem;

class FakeEngine final : public woki::ext::wasm::Engine {
public:
    [[nodiscard]] woki::Result<void> Load(woki::ext::Record&, woki::ext::host::HostApi&) override {
        loaded = true;
        return woki::Ok();
    }

    [[nodiscard]] woki::Result<woki::u32> ApiVersion(woki::ext::Record&) override {
        return woki::Ok(api_version);
    }

    [[nodiscard]] woki::Result<woki::i32> Init(woki::ext::Record&) override {
        initialized = true;
        return woki::Ok(init_result);
    }

    [[nodiscard]] woki::Result<void> Tick(woki::ext::Record&, woki::f64 delta_ms) override {
        last_delta_ms = delta_ms;
        ++ticks;
        if (fail_tick) {
            return woki::Err(woki::ErrorCode::InvalidState, "ext_on_tick trap");
        }
        return woki::Ok();
    }

    [[nodiscard]] woki::Result<void> Event(woki::ext::Record&, woki::u32 event_type, std::span<const woki::u8>) override {
        last_event_type = event_type;
        if (fail_event) {
            return woki::Err(woki::ErrorCode::InvalidState, "ext_on_event trap");
        }
        return woki::Ok();
    }

    [[nodiscard]] woki::Result<woki::i32> Command(woki::ext::Record&, std::string_view command_id, std::span<const woki::u8> payload) override {
        last_command_id = command_id;
        last_command_payload_size = payload.size();
        if (fail_command) {
            return woki::Err(woki::ErrorCode::InvalidState, "ext_on_command trap");
        }
        return woki::Ok(command_result);
    }

    void Unload(woki::ext::Record&) override {
        unloaded = true;
    }

    woki::u32 api_version{woki::ext::kApiVersion};
    woki::i32 init_result{0};
    woki::i32 command_result{0};
    bool fail_tick{false};
    bool fail_event{false};
    bool fail_command{false};
    bool loaded{false};
    bool initialized{false};
    bool unloaded{false};
    int ticks{0};
    woki::f64 last_delta_ms{0.0};
    woki::u32 last_event_type{0};
    std::string last_command_id;
    std::size_t last_command_payload_size{0};
};

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_wasm_backend_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteWasmMagic(const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    const std::array<unsigned char, 8> header{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    output.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
}

[[nodiscard]] woki::ext::Record MakeRecord(const fs::path& root) {
    woki::ext::Record record;
    record.id = "woki.hello";
    record.state = woki::ext::State::PermissionChecked;
    record.manifest.id = "woki.hello";
    record.manifest.name = "Hello";
    record.manifest.version = "0.1.0";
    record.manifest.api_version = woki::ext::kApiVersion;
    record.manifest.permissions = {woki::ext::Permission::Log};
    record.package.install_root = root;
    record.package.manifest = root / "manifest.yaml";
    record.package.wasm = root / "extension.wasm";
    record.package.data_root = root / "data";
    record.package.cache_root = root / "cache";
    return record;
}

void WriteManifest(const fs::path& path) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << "id: woki.hello\n";
}

} // namespace

TEST_CASE("Wasm backend loads validates api version and initializes through engine") {
    const fs::path root = MakeTempDir("valid");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    FakeEngine* engine_ptr = engine.get();
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    auto loaded = backend.Load(record);
    REQUIRE(loaded.has_value());
    REQUIRE(engine_ptr->loaded);
    REQUIRE(record.tier == woki::ext::RuntimeTier::Wasm);

    auto initialized = backend.Initialize(record);
    REQUIRE(initialized.has_value());
    REQUIRE(engine_ptr->initialized);

    backend.Tick(record, 10.0);
    REQUIRE(engine_ptr->ticks == 1);
    REQUIRE(engine_ptr->last_delta_ms == 10.0);

    backend.DispatchEvent(record, 7, {});
    REQUIRE(engine_ptr->last_event_type == 7);

    const std::array<woki::u8, 1> payload{7};
    auto commanded = backend.DispatchCommand(record, "woki.hello.say", payload);
    REQUIRE(commanded.has_value());
    REQUIRE(engine_ptr->last_command_id == "woki.hello.say");
    REQUIRE(engine_ptr->last_command_payload_size == payload.size());

    backend.Unload(record);
    REQUIRE(engine_ptr->unloaded);
}

TEST_CASE("Wasm backend reports extension command failures") {
    const fs::path root = MakeTempDir("command_fail");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    engine->command_result = -1;
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    REQUIRE(backend.Load(record).has_value());
    REQUIRE(backend.Initialize(record).has_value());
    auto commanded = backend.DispatchCommand(record, "woki.hello.say", {});
    REQUIRE_FALSE(commanded.has_value());
    REQUIRE(commanded.error().Message().contains("failure code"));
}

TEST_CASE("Wasm backend rejects invalid wasm magic before engine load") {
    const fs::path root = MakeTempDir("bad_magic");
    WriteManifest(root / "manifest.yaml");
    {
        std::ofstream output(root / "extension.wasm", std::ios::binary);
        REQUIRE(output.good());
        output << "bad";
    }

    auto engine = woki::createScope<FakeEngine>();
    FakeEngine* engine_ptr = engine.get();
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    auto loaded = backend.Load(record);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE_FALSE(engine_ptr->loaded);
    REQUIRE(loaded.error().Code() == woki::ErrorCode::ParseInvalidFormat);
}

TEST_CASE("Wasm backend rejects api version mismatch") {
    const fs::path root = MakeTempDir("api_mismatch");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    engine->api_version = 999;
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    auto loaded = backend.Load(record);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().Message().contains("apiVersion mismatch"));
}

TEST_CASE("Wasm backend reports ext_init failures") {
    const fs::path root = MakeTempDir("init_fail");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    engine->init_result = -1;
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    REQUIRE(backend.Load(record).has_value());
    auto initialized = backend.Initialize(record);
    REQUIRE_FALSE(initialized.has_value());
    REQUIRE(initialized.error().Message().contains("ext_init"));
}

TEST_CASE("Wasm backend marks active records failed when tick returns an error") {
    const fs::path root = MakeTempDir("tick_fail");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    engine->fail_tick = true;
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    REQUIRE(backend.Load(record).has_value());
    REQUIRE(backend.Initialize(record).has_value());
    record.state = woki::ext::State::Active;

    backend.Tick(record, 16.0);
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error.contains("ext_on_tick"));
}

TEST_CASE("Wasm backend marks active records failed when event dispatch returns an error") {
    const fs::path root = MakeTempDir("event_fail");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");

    auto engine = woki::createScope<FakeEngine>();
    engine->fail_event = true;
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);

    REQUIRE(backend.Load(record).has_value());
    REQUIRE(backend.Initialize(record).has_value());
    record.state = woki::ext::State::Active;

    backend.DispatchEvent(record, 7, {});
    REQUIRE(record.state == woki::ext::State::Failed);
    REQUIRE(record.error == "ext_on_event trap");
}

TEST_CASE("Wasm backend rejects oversized event and command payloads") {
    const fs::path root = MakeTempDir("payload_limits");
    WriteManifest(root / "manifest.yaml");
    WriteWasmMagic(root / "extension.wasm");
    auto engine = woki::createScope<FakeEngine>();
    FakeEngine* engine_ptr = engine.get();
    woki::ext::wasm::Backend backend(std::move(engine));
    auto record = MakeRecord(root);
    REQUIRE(backend.Load(record).has_value());
    REQUIRE(backend.Initialize(record).has_value());
    record.state = woki::ext::State::Active;
    woki::ext::Runtime runtime(&backend);

    const std::vector<woki::u8> oversized(woki::ext::limits::kMaxEventBytes + 1);
    runtime.DispatchEvent(record, 1, oversized);
    REQUIRE(record.state == woki::ext::State::Active);
    REQUIRE(engine_ptr->last_event_type == 0);

    auto command = runtime.DispatchCommand(record, "woki.hello.say", oversized);
    REQUIRE_FALSE(command.has_value());
    REQUIRE(command.error().Code() == woki::ErrorCode::ValidationOutOfRange);
    REQUIRE(record.state == woki::ext::State::Active);
    REQUIRE_FALSE(engine_ptr->unloaded);
}
