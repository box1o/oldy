#include <fstream>
#include <sstream>
#include <stdexcept>
#include <catch2/catch_test_macros.hpp>

#include "cli_internal.hpp"

namespace {

class UnusedProcessRunner final : public wokiext::ProcessRunner {
public:
    bool Run(std::span<const std::string>, wokiext::Diagnostics&) override {
        return false;
    }
};

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : storage(Create()),
          path(storage.Path()) {}

private:
    static wokiext::TemporaryDirectory Create() {
        auto temporary = wokiext::TemporaryDirectory::Create("wokiext-test-");
        if (!temporary)
            throw std::runtime_error(temporary.error());
        return std::move(*temporary);
    }

    wokiext::TemporaryDirectory storage;

public:
    std::filesystem::path path;
};

TEST_CASE("parser creates typed commands with defaults") {
    constexpr std::string_view args[]{"wokiext", "create", "My Extension", "--lang", "c"};
    auto parsed = wokiext::ParseCommand(args, "/work");
    REQUIRE(parsed);
    const auto& command = std::get<wokiext::CreateCommand>(*parsed);
    CHECK(command.options.name == "My Extension");
    CHECK(command.options.lang == "c");
    CHECK(command.options.out_dir == "/work");
}

TEST_CASE("parser preserves build modes and reports usage errors") {
    constexpr std::string_view args[]{"tool", "run", "project", "--debug"};
    auto parsed = wokiext::ParseCommand(args, ".");
    REQUIRE(parsed);
    const auto& command = std::get<wokiext::RunCommand>(*parsed);
    CHECK(command.options.config == "Debug");
    CHECK(command.options.executable == "tool");

    constexpr std::string_view invalid[]{"tool", "build", "project", "--bad"};
    auto error = wokiext::ParseCommand(invalid, ".");
    REQUIRE_FALSE(error);
    CHECK(error.error().message == "Unknown option: --bad");
}

TEST_CASE("parser supports named configurations and option terminators") {
    constexpr std::string_view configured[]{"tool", "build", "project", "--config", "RelWithDebInfo"};
    auto parsed = wokiext::ParseCommand(configured, ".");
    REQUIRE(parsed);
    CHECK(std::get<wokiext::BuildCommand>(*parsed).options.config == "RelWithDebInfo");

    constexpr std::string_view terminated[]{"tool", "verify", "--", "--project"};
    parsed = wokiext::ParseCommand(terminated, ".");
    REQUIRE(parsed);
    CHECK(std::get<wokiext::VerifyCommand>(*parsed).options.path == "--project");

    constexpr std::string_view conflicting[]{"tool", "build", "project", "--debug", "--config", "Release"};
    auto error = wokiext::ParseCommand(conflicting, ".");
    REQUIRE_FALSE(error);
    CHECK(error.error().message.find("Specify only one") != std::string::npos);
}

TEST_CASE("parser rejects options and empty strings as option values") {
    constexpr std::string_view flag_value[]{"tool", "install", "archive", "--root", "--force"};
    auto parsed = wokiext::ParseCommand(flag_value, ".");
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "Option requires a value: --root");

    constexpr std::string_view empty_value[]{"tool", "bundle", "project", "--out="};
    parsed = wokiext::ParseCommand(empty_value, ".");
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "Option requires a non-empty value: --out");
}

TEST_CASE("diagnostics separates normal output and errors") {
    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    diagnostics.Info("created");
    diagnostics.Warning("careful");
    diagnostics.Error("failed");
    CHECK(output.str() == "created\n");
    CHECK(error.str() == "Warning: careful\nfailed\n");
}

TEST_CASE("build arguments are deterministic and preserve path boundaries") {
    const auto root = std::filesystem::absolute("project with spaces").lexically_normal();
    wokiext::BuildOptions options{.path = root, .executable = "wokiext", .config = "Debug"};
    const auto configure = wokiext::BuildConfigureArguments(options, "/cmake modules", "/sdk path");
    REQUIRE(configure.size() == 12);
    CHECK(configure[0] == "cmake");
    CHECK(configure[1] == "-G");
    CHECK(configure[2] == "Ninja");
    CHECK(configure[4] == (root / "build").generic_string());
    CHECK(configure[6] == root.generic_string());
    CHECK(configure[7] == "-DCMAKE_BUILD_TYPE=Debug");
    CHECK(configure[9] == "-DWOKI_CMAKE_DIR=/cmake modules");
    CHECK(configure[10] == "-DWOKI_SDK_DIR=/sdk path");
    CHECK(configure[11] == "-DWOKI_EXTENSION_PACKAGE_DIR=" + (root / "build/packages").generic_string());

    CHECK(wokiext::BuildCompileArguments(options) == std::vector<std::string>{"cmake", "--build", (root / "build").generic_string(), "--config", "Debug"});
}

TEST_CASE("parser rejects duplicate singleton options") {
    constexpr std::string_view values[]{"tool", "bundle", "project", "--out", "one", "--out=two"};
    auto parsed = wokiext::ParseCommand(values, ".");
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "Option may be specified only once: --out");

    constexpr std::string_view flags[]{"tool", "install", "archive", "--force", "--force"};
    parsed = wokiext::ParseCommand(flags, ".");
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "Option may be specified only once: --force");
}

TEST_CASE("system process runner reports process success and failure") {
    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    wokiext::SystemProcessRunner processes;
    CHECK(processes.Run(std::vector<std::string>{"cmake", "-E", "true"}, diagnostics));
    CHECK_FALSE(processes.Run(std::vector<std::string>{"cmake", "-E", "false"}, diagnostics));
    CHECK_FALSE(processes.Run(std::vector<std::string>{"wokiext-command-that-does-not-exist"}, diagnostics));
    CHECK(error.str().find("exited with code") != std::string::npos);
    CHECK(error.str().find("Failed to start") != std::string::npos);
}

TEST_CASE("temporary directories are private and removed by scope") {
    std::filesystem::path path;
    {
        auto temporary = wokiext::TemporaryDirectory::Create("wokiext-permissions-");
        REQUIRE(temporary);
        path = temporary->Path();
#ifndef _WIN32
        const auto permissions = std::filesystem::status(path).permissions();
        CHECK((permissions & std::filesystem::perms::group_all) == std::filesystem::perms::none);
        CHECK((permissions & std::filesystem::perms::others_all) == std::filesystem::perms::none);
#endif
    }
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("clean removes only generated project artifacts") {
    TemporaryDirectory temporary;
    const auto project = temporary.path / "project";
    std::filesystem::create_directories(project / "build");
    std::ofstream(project / "CMakeLists.txt") << "project(test)";
    std::ofstream(project / "manifest.yaml") << "id: test";
    std::ofstream(project / "source.txt") << "keep";

    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    UnusedProcessRunner processes;
    wokiext::SystemFilesystem filesystem;
    wokiext::Context context{diagnostics, processes, filesystem};
    CHECK(wokiext::Clean(context, {.path = project}) == wokiext::Status::Ok);
    CHECK_FALSE(std::filesystem::exists(project / "build"));
    CHECK(std::filesystem::exists(project / "source.txt"));
}

TEST_CASE("clean rejects non-project directories") {
    TemporaryDirectory temporary;
    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    UnusedProcessRunner processes;
    wokiext::SystemFilesystem filesystem;
    wokiext::Context context{diagnostics, processes, filesystem};
    CHECK(wokiext::Clean(context, {.path = temporary.path}) == wokiext::Status::Error);
    CHECK(error.str().find("Clean expects an extension project directory") != std::string::npos);
}

TEST_CASE("clean rejects symbolic-link project roots") {
    TemporaryDirectory temporary;
    const auto project = temporary.path / "project";
    const auto link = temporary.path / "project-link";
    std::filesystem::create_directories(project);
    std::ofstream(project / "CMakeLists.txt") << "project(test)";
    std::ofstream(project / "manifest.yaml") << "id: test";
    std::error_code link_error;
    std::filesystem::create_directory_symlink(project, link, link_error);
    if (link_error)
        SKIP("directory symlinks are unavailable: " + link_error.message());

    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    UnusedProcessRunner processes;
    wokiext::SystemFilesystem filesystem;
    wokiext::Context context{diagnostics, processes, filesystem};
    CHECK(wokiext::Clean(context, {.path = link / "."}) == wokiext::Status::Error);
    CHECK(std::filesystem::exists(project / "CMakeLists.txt"));
    CHECK(error.str().find("symbolic-link") != std::string::npos);
}

TEST_CASE("create validates ids before writing and quotes YAML names") {
    TemporaryDirectory temporary;
    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    UnusedProcessRunner processes;
    wokiext::SystemFilesystem filesystem;
    wokiext::Context context{diagnostics, processes, filesystem};

    CHECK(wokiext::Create(context, {.name = "Invalid", .id = "../escape", .out_dir = temporary.path, .lang = "c"}) == wokiext::Status::Usage);
    CHECK_FALSE(std::filesystem::exists(temporary.path / "invalid"));

    CHECK(wokiext::Create(context, {.name = "True: Bob's Tool", .id = "woki.bobs-tool", .out_dir = temporary.path, .lang = "c"}) == wokiext::Status::Ok);
    std::ifstream manifest(temporary.path / "true-bob-s-tool" / "manifest.yaml");
    const std::string contents{std::istreambuf_iterator<char>(manifest), std::istreambuf_iterator<char>()};
    CHECK(contents.find("name: 'True: Bob''s Tool'") != std::string::npos);
    CHECK(contents.find("activation:\n  startup: true") != std::string::npos);
}

TEST_CASE("remove deletes config by default and keep-data preserves all state") {
    TemporaryDirectory temporary;
    std::ostringstream output;
    std::ostringstream error;
    wokiext::Diagnostics diagnostics(output, error);
    UnusedProcessRunner processes;
    wokiext::SystemFilesystem filesystem;
    wokiext::Context context{diagnostics, processes, filesystem};

    struct {
        std::filesystem::path extensions;
        std::filesystem::path data;
        std::filesystem::path config;
        std::filesystem::path cache;
    } roots{temporary.path / "extensions", temporary.path / "ext-data", temporary.path / "config" / "ext", temporary.path / "cache" / "ext"};

    for (const auto& root : {roots.extensions, roots.data, roots.config, roots.cache})
        std::filesystem::create_directories(root / "woki.remove");
    CHECK(wokiext::Remove(context, {.id = "woki.remove", .root = temporary.path}) == wokiext::Status::Ok);
    for (const auto& root : {roots.extensions, roots.data, roots.config, roots.cache})
        CHECK_FALSE(std::filesystem::exists(root / "woki.remove"));

    for (const auto& root : {roots.extensions, roots.data, roots.config, roots.cache})
        std::filesystem::create_directories(root / "woki.keep");
    CHECK(wokiext::Remove(context, {.id = "woki.keep", .root = temporary.path, .keep_data = true}) == wokiext::Status::Ok);
    CHECK_FALSE(std::filesystem::exists(roots.extensions / "woki.keep"));
    CHECK(std::filesystem::exists(roots.data / "woki.keep"));
    CHECK(std::filesystem::exists(roots.config / "woki.keep"));
    CHECK(std::filesystem::exists(roots.cache / "woki.keep"));
    CHECK(output.str().find("kept data, config, and cache") != std::string::npos);
}

} // namespace
