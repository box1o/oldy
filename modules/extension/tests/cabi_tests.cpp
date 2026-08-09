#include <array>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_cabi_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

[[nodiscard]] woki::ext::Record MakeRecord(const fs::path& root) {
    woki::ext::Record record;
    record.id = "woki.hello";
    record.manifest.id = "woki.hello";
    record.manifest.name = "Hello";
    record.manifest.version = "0.1.0";
    record.manifest.permissions = {
        woki::ext::Permission::Log,
        woki::ext::Permission::Paths,
        woki::ext::Permission::Storage,
    };
    record.package.data_root = root / "data";
    record.package.cache_root = root / "cache";
    return record;
}

} // namespace

TEST_CASE("Extension C ABI host_log returns denied without permission") {
    auto record = MakeRecord(MakeTempDir("log_denied"));
    record.manifest.permissions.clear();

    const char message[] = "hello";
    const auto status = woki::ext::host::cabi::Log(record, 1, message, 5);
    REQUIRE(status == woki::ext::host::cabi::kDenied);
}

TEST_CASE("Extension C ABI preserves denied status for host permission failures") {
    auto record = MakeRecord(MakeTempDir("path_denied_status"));
    record.manifest.permissions.clear();
    std::array<char, 32> output{};

    const auto status = woki::ext::host::cabi::PathData(record, output.data(), static_cast<woki::u32>(output.size()));
    REQUIRE(status == woki::ext::host::cabi::kDenied);
}

TEST_CASE("Extension C ABI path calls copy null terminated paths") {
    auto record = MakeRecord(MakeTempDir("paths"));
    std::array<char, 512> output{};

    auto status = woki::ext::host::cabi::PathData(record, output.data(), static_cast<woki::u32>(output.size()));
    REQUIRE(status == 0);
    REQUIRE(std::string_view(output.data()).contains("data"));

    std::array<char, 2> small{};
    status = woki::ext::host::cabi::PathCache(record, small.data(), static_cast<woki::u32>(small.size()));
    REQUIRE(status == -3);
}

TEST_CASE("Extension C ABI file read and write use extension storage") {
    auto record = MakeRecord(MakeTempDir("storage"));
    const std::array<woki::u8, 3> input{4, 5, 6};

    auto status = woki::ext::host::cabi::FileWrite(record, "state/data.bin", input.data(), static_cast<woki::u32>(input.size()));
    REQUIRE(status == 0);

    std::array<woki::u8, 3> output{};
    woki::u32 output_len = static_cast<woki::u32>(output.size());
    status = woki::ext::host::cabi::FileRead(record, "state/data.bin", output.data(), &output_len);
    REQUIRE(status == 0);
    REQUIRE(output_len == input.size());
    REQUIRE(output == input);

    std::array<woki::u8, 1> small{};
    output_len = static_cast<woki::u32>(small.size());
    status = woki::ext::host::cabi::FileRead(record, "state/data.bin", small.data(), &output_len);
    REQUIRE(status == -3);
    REQUIRE(output_len == input.size());
}

TEST_CASE("Extension C ABI file paths reject traversal") {
    auto record = MakeRecord(MakeTempDir("traversal"));
    woki::u32 output_len = 0;
    auto status = woki::ext::host::cabi::FileRead(record, "../outside.bin", nullptr, &output_len);
    REQUIRE(status == woki::ext::host::cabi::kInvalid);
}

TEST_CASE("Extension C ABI file append keeps existing contents") {
    auto record = MakeRecord(MakeTempDir("append"));
    const std::array<woki::u8, 3> first{'a', 'b', 'c'};
    const std::array<woki::u8, 3> second{'d', 'e', 'f'};

    auto status = woki::ext::host::cabi::FileAppend(record, "events.log", first.data(), static_cast<woki::u32>(first.size()));
    REQUIRE(status == 0);
    status = woki::ext::host::cabi::FileAppend(record, "events.log", second.data(), static_cast<woki::u32>(second.size()));
    REQUIRE(status == 0);

    std::array<woki::u8, 6> output{};
    woki::u32 output_len = static_cast<woki::u32>(output.size());
    status = woki::ext::host::cabi::FileRead(record, "events.log", output.data(), &output_len);
    REQUIRE(status == 0);
    REQUIRE(output_len == output.size());
    REQUIRE(std::string_view(reinterpret_cast<const char*>(output.data()), output.size()) == "abcdef");
}
