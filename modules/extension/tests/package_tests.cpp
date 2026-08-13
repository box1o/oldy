#ifndef __EMSCRIPTEN__
#include <archive.h>
#include <archive_entry.h>
#endif

#include <array>
#include <thread>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/package.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_package_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.good());
    if (path.filename() == "manifest.yaml" && !contents.contains("$schema:"))
        output << "$schema: https://schemas.woki.dev/extension.manifest/v1.schema.json\n";
    output << contents;
}

[[nodiscard]] woki::ext::Manifest MakeManifest() {
    woki::ext::Manifest manifest;
    manifest.id = "woki.hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log};
    return manifest;
}

} // namespace

TEST_CASE("Extension package layout resolves canonical roots") {
    const fs::path root = MakeTempDir("layout");
    const woki::ext::Manifest manifest = MakeManifest();

    auto layout = woki::ext::ResolvePackageLayout(manifest, root / "extensions", root / "ext-data", root / "cache" / "woki" / "ext");

    REQUIRE(layout.has_value());
    REQUIRE(layout->install_root.filename() == manifest.id);
    REQUIRE(layout->manifest == layout->install_root / "manifest.yaml");
    REQUIRE(layout->wasm == layout->install_root / "extension.wasm");
    REQUIRE(layout->data_root == fs::weakly_canonical(root / "ext-data" / manifest.id));
    REQUIRE(layout->config_root == fs::weakly_canonical(root / "ext-config" / manifest.id));
    REQUIRE(layout->cache_root == fs::weakly_canonical(root / "cache" / "woki" / "ext" / manifest.id));
    REQUIRE(layout->config_root != layout->data_root);
    REQUIRE(layout->config_root != layout->cache_root);
}

TEST_CASE("Extension roots keep config separate from extension data") {
    const fs::path root = MakeTempDir("roots");
    const auto roots = woki::ext::RootsFromBase(root);
    REQUIRE(roots);
    REQUIRE(roots->data == fs::absolute(root).lexically_normal() / "ext-data");
    REQUIRE(roots->config == fs::absolute(root).lexically_normal() / "config" / "ext");
    REQUIRE(roots->config != roots->data);
}

TEST_CASE("Extension package layout rejects empty roots") {
    auto layout = woki::ext::ResolvePackageLayout(MakeManifest(), {}, "data", "cache");
    REQUIRE_FALSE(layout.has_value());
    REQUIRE(layout.error().Code() == woki::ErrorCode::InvalidArgument);
}

TEST_CASE("Extension roots reject overlap, non-canonical paths, and symlink aliases") {
    const fs::path root = MakeTempDir("invalid_roots");
    const woki::ext::Roots valid{root / "extensions", root / "data", root / "cache", root / "config"};
    REQUIRE(woki::ext::ValidateRoots(valid));

    auto overlapping = valid;
    overlapping.cache = overlapping.data / "cache";
    const auto overlap_result = woki::ext::ValidateRoots(overlapping);
    REQUIRE_FALSE(overlap_result);
    REQUIRE(overlap_result.error().Message().contains("non-overlapping"));

    auto non_canonical = valid;
    non_canonical.cache = root / "missing" / ".." / "cache";
    REQUIRE_FALSE(woki::ext::ValidateRoots(non_canonical));

    fs::create_directories(root / "real");
    std::error_code error;
    fs::create_directory_symlink(root / "real", root / "alias", error);
    if (!error) {
        auto aliased = valid;
        aliased.extensions = root / "alias" / "extensions";
        const auto alias_result = woki::ext::ValidateRoots(aliased);
        REQUIRE_FALSE(alias_result);
        REQUIRE(alias_result.error().Code() == woki::ErrorCode::FileAccessDenied);
    }
}

TEST_CASE("Extension package layout validates existing manifest and wasm") {
    const fs::path root = MakeTempDir("valid_package");
    const woki::ext::Manifest manifest = MakeManifest();
    auto layout = woki::ext::ResolvePackageLayout(manifest, root / "extensions", root / "ext-data", root / "cache" / "woki" / "ext");
    REQUIRE(layout.has_value());

    fs::create_directories(layout->install_root);
    WriteFile(layout->manifest, "id: woki.hello\n");
    WriteFile(layout->wasm, "");

    auto valid = woki::ext::ValidatePackageLayout(*layout);
    REQUIRE(valid.has_value());
}

TEST_CASE("Extension package layout rejects missing wasm") {
    const fs::path root = MakeTempDir("missing_wasm");
    const woki::ext::Manifest manifest = MakeManifest();
    auto layout = woki::ext::ResolvePackageLayout(manifest, root / "extensions", root / "ext-data", root / "cache" / "woki" / "ext");
    REQUIRE(layout.has_value());

    fs::create_directories(layout->install_root);
    WriteFile(layout->manifest, "id: woki.hello\n");

    auto valid = woki::ext::ValidatePackageLayout(*layout);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::FileNotFound);
    REQUIRE(valid.error().Message().contains("extension.wasm"));
    REQUIRE(valid.error().Message().contains("runtime.wasm"));
}

TEST_CASE("Extension package layout rejects a symlinked wasm module") {
    const fs::path root = MakeTempDir("symlinked_wasm");
    const woki::ext::Manifest manifest = MakeManifest();
    auto layout = woki::ext::ResolvePackageLayout(manifest, root / "extensions", root / "ext-data", root / "cache");
    REQUIRE(layout.has_value());
    fs::create_directories(layout->install_root);
    WriteFile(layout->manifest, "id: woki.hello\n");
    WriteFile(root / "outside.wasm", "");
    std::error_code error;
    fs::create_symlink(root / "outside.wasm", layout->wasm, error);
    if (error) {
        SKIP("File symlinks are unavailable");
    }

    auto valid = woki::ext::ValidatePackageLayout(*layout);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::FileAccessDenied);
}

TEST_CASE("Extension package installer installs an unpacked package through staging") {
    const fs::path root = MakeTempDir("install_unpacked");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(source / "extension.wasm", "");
    WriteFile(source / "assets" / "icon.txt", "icon");
    WriteFile(source / ".woki-state", "internal build metadata");

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE(installed.has_value());
    REQUIRE(installed->install_root == fs::weakly_canonical(roots.extensions / "woki.hello"));
    REQUIRE(fs::is_regular_file(installed->manifest));
    REQUIRE(fs::is_regular_file(installed->wasm));
    REQUIRE(fs::is_regular_file(installed->install_root / "assets" / "icon.txt"));
    REQUIRE_FALSE(fs::exists(installed->install_root / ".woki-state"));
}

TEST_CASE("Extension package installer rejects a source overlapping its install root") {
    const fs::path root = MakeTempDir("source_install_overlap");
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};
    const fs::path source = roots.extensions / "woki.hello";
    fs::create_directories(source);
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "source");

    const auto installed = woki::ext::InstallUnpackedPackage(source, roots, woki::ext::InstallPolicy::ReplaceExisting);
    REQUIRE_FALSE(installed);
    REQUIRE(installed.error().Message().contains("overlap"));
    REQUIRE(fs::is_regular_file(source / "extension.wasm"));
}

TEST_CASE("Extension package installer deterministically recovers stale transactions") {
    const fs::path root = MakeTempDir("stale_transactions");
    const fs::path source = root / "source";
    fs::create_directories(source);
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "candidate");
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};
    fs::create_directories(roots.extensions / ".woki.hello.1.0.replaced");
    fs::create_directories(roots.extensions / ".woki.hello.2.0.replaced");
    fs::create_directories(roots.extensions / ".woki.hello.3.0.installing");
    WriteFile(roots.extensions / ".woki.hello.1.0.replaced" / "marker", "older");
    WriteFile(roots.extensions / ".woki.hello.2.0.replaced" / "marker", "newer");

    const auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(installed);
    REQUIRE(installed.error().Message().contains("already installed"));
    REQUIRE(fs::is_regular_file(roots.extensions / "woki.hello" / "marker"));
    REQUIRE_FALSE(fs::exists(roots.extensions / ".woki.hello.1.0.replaced"));
    REQUIRE_FALSE(fs::exists(roots.extensions / ".woki.hello.2.0.replaced"));
    REQUIRE_FALSE(fs::exists(roots.extensions / ".woki.hello.3.0.installing"));
}

TEST_CASE("Extension package install lock serializes competing commits") {
    const fs::path root = MakeTempDir("concurrent_install");
    const fs::path source = root / "source";
    fs::create_directories(source);
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "module");
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};
    std::array<bool, 2> succeeded{};
    std::thread first([&] { succeeded[0] = woki::ext::InstallUnpackedPackage(source, roots).has_value(); });
    std::thread second([&] { succeeded[1] = woki::ext::InstallUnpackedPackage(source, roots).has_value(); });
    first.join();
    second.join();

    REQUIRE(succeeded[0] != succeeded[1]);
    REQUIRE(fs::is_regular_file(roots.extensions / "woki.hello" / "extension.wasm"));
    for (const fs::directory_entry& entry : fs::directory_iterator(roots.extensions))
        REQUIRE_FALSE(entry.path().filename().string().ends_with(".installing"));
}

TEST_CASE("Extension package installer refuses to overwrite installed packages") {
    const fs::path root = MakeTempDir("install_overwrite");
    const fs::path source = root / "source";
    fs::create_directories(source);

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(source / "extension.wasm", "");

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    REQUIRE(woki::ext::InstallUnpackedPackage(source, roots).has_value());
    auto second = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(second.has_value());
    REQUIRE(second.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(second.error().Message().contains("already installed"));
}

TEST_CASE("Extension package installer explicitly replaces package files without touching state roots") {
    const fs::path root = MakeTempDir("install_replace");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");
    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");
    WriteFile(source / "extension.wasm", "v1");
    WriteFile(source / "assets" / "version.txt", "v1");
    const woki::ext::Roots roots{root / "extensions", root / "ext-data", root / "cache" / "ext"};

    auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE(installed.has_value());
    fs::create_directories(installed->data_root);
    fs::create_directories(installed->config_root);
    fs::create_directories(installed->cache_root);
    WriteFile(installed->data_root / "state", "data");
    WriteFile(installed->config_root / "settings", "config");
    WriteFile(installed->cache_root / "entry", "cache");

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 2.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");
    WriteFile(source / "extension.wasm", "v2");
    WriteFile(source / "assets" / "version.txt", "v2");
    auto replaced = woki::ext::InstallUnpackedPackage(source, roots, woki::ext::InstallPolicy::ReplaceExisting);
    REQUIRE(replaced.has_value());
    REQUIRE(fs::file_size(replaced->wasm) == 2);
    REQUIRE(fs::is_regular_file(replaced->data_root / "state"));
    REQUIRE(fs::is_regular_file(replaced->config_root / "settings"));
    REQUIRE(fs::is_regular_file(replaced->cache_root / "entry"));
}

TEST_CASE("Extension package replacement preserves the old install when staging validation fails") {
    const fs::path root = MakeTempDir("replace_failure");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");
    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");
    WriteFile(source / "extension.wasm", "old");
    const woki::ext::Roots roots{root / "extensions", root / "ext-data", root / "cache" / "ext"};
    REQUIRE(woki::ext::InstallUnpackedPackage(source, roots).has_value());

    WriteFile(source / "signature", "unverified");
    auto replaced = woki::ext::InstallUnpackedPackage(source, roots, woki::ext::InstallPolicy::ReplaceExisting);
    REQUIRE_FALSE(replaced.has_value());
    REQUIRE(fs::file_size(roots.extensions / "woki.hello" / "extension.wasm") == 3);
    REQUIRE_FALSE(fs::exists(roots.extensions / "woki.hello" / "signature"));
}

TEST_CASE("Extension package factory rejects inconsistent identity and layout") {
    auto manifest = MakeManifest();
    const fs::path root = MakeTempDir("package_factory");
    const woki::ext::PackageLayout layout{root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"};
    REQUIRE_FALSE(woki::ext::ExtensionPackage::Create("woki.other", manifest, layout));
    auto inconsistent = layout;
    inconsistent.wasm = root / "other.wasm";
    REQUIRE_FALSE(woki::ext::ExtensionPackage::Create(manifest.id, manifest, inconsistent));
    REQUIRE(woki::ext::ExtensionPackage::Create(manifest.id, manifest, layout));
}

TEST_CASE("Extension package replacement treats retained backup cleanup as a committed success") {
    const fs::path root = MakeTempDir("replace_backup_cleanup");
    const fs::path source = root / "source";
    fs::create_directories(source);
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "old");
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};
    auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE(installed);
    fs::permissions(installed->install_root, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
    WriteFile(source / "extension.wasm", "new");

    auto replaced = woki::ext::InstallUnpackedPackage(source, roots, woki::ext::InstallPolicy::ReplaceExisting);
    CHECK(replaced.has_value());
    CHECK(fs::is_regular_file(roots.extensions / "woki.hello" / "extension.wasm"));
    for (const fs::directory_entry& entry : fs::directory_iterator(roots.extensions)) {
        if (entry.path().filename().string().contains(".replaced"))
            fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::replace);
    }
}

TEST_CASE("Extension package installer rejects unsupported entries") {
    const fs::path root = MakeTempDir("install_unsupported");
    const fs::path source = root / "source";
    fs::create_directories(source);

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(source / "extension.wasm", "");
    WriteFile(source / "random.txt", "bad");

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(installed.error().Message().contains("unsupported"));
}

TEST_CASE("Extension package installer rejects unpacked symlinks without installing their targets") {
    const fs::path root = MakeTempDir("install_unpacked_symlink");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "module");
    WriteFile(root / "outside-secret", "must-not-be-installed");
    std::error_code error;
    fs::create_symlink(root / "outside-secret", source / "assets" / "secret", error);
    if (error)
        SKIP("File symlinks are unavailable");
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};

    const auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(installed);
    REQUIRE(installed.error().Message().contains("symlink"));
    REQUIRE_FALSE(fs::exists(roots.extensions / "woki.hello"));
    for (const fs::directory_entry& entry : fs::directory_iterator(roots.extensions))
        REQUIRE_FALSE(entry.path().filename().string().ends_with(".installing"));
    REQUIRE(fs::file_size(root / "outside-secret") == 21);
}

TEST_CASE("Extension package installer enforces the single-file limit before commit") {
    const fs::path root = MakeTempDir("install_unpacked_file_limit");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "module");
    WriteFile(source / "assets" / "oversized.bin", "x");
    fs::resize_file(source / "assets" / "oversized.bin", 64u * 1024u * 1024u + 1u);
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};

    const auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(installed);
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationOutOfRange);
    REQUIRE_FALSE(fs::exists(roots.extensions / "woki.hello"));
}

TEST_CASE("Extension package installer explicitly rejects native and signature payloads") {
    const fs::path root = MakeTempDir("install_policy_entries");
    const fs::path source = root / "source";
    fs::create_directories(source / "extension.native");
    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");
    WriteFile(source / "extension.wasm", "");
    WriteFile(source / "extension.native" / "plugin.so", "native");
    WriteFile(source / "signature", "unsigned-policy");
    const woki::ext::Roots roots{root / "extensions", root / "ext-data", root / "cache" / "ext"};

    auto installed = woki::ext::InstallUnpackedPackage(source, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Message().contains("native payloads and signatures are unsupported"));
}

#ifndef __EMSCRIPTEN__

void WriteZipPackage(const fs::path& archive_path, const fs::path& source_root) {
    struct archive* writer = archive_write_new();
    REQUIRE(writer != nullptr);
    REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
#ifdef _WIN32
    REQUIRE(archive_write_open_filename_w(writer, archive_path.c_str()) == ARCHIVE_OK);
#else
    REQUIRE(archive_write_open_filename(writer, archive_path.c_str()) == ARCHIVE_OK);
#endif

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root, fs::directory_options::skip_permission_denied)) {
        const fs::path relative = fs::relative(entry.path(), source_root);
        if (relative.empty() || relative == ".") {
            continue;
        }

        struct archive_entry* archive_entry_ptr = archive_entry_new();
        REQUIRE(archive_entry_ptr != nullptr);

        if (entry.is_directory()) {
            std::string pathname = relative.generic_string();
            if (pathname.back() != '/') {
                pathname += '/';
            }
            archive_entry_set_pathname(archive_entry_ptr, pathname.c_str());
            archive_entry_set_filetype(archive_entry_ptr, AE_IFDIR);
            archive_entry_set_perm(archive_entry_ptr, 0755);
            REQUIRE(archive_write_header(writer, archive_entry_ptr) == ARCHIVE_OK);
        } else {
            std::ifstream input(entry.path(), std::ios::binary);
            REQUIRE(input.good());
            const std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
            archive_entry_set_pathname(archive_entry_ptr, relative.generic_string().c_str());
            archive_entry_set_filetype(archive_entry_ptr, AE_IFREG);
            archive_entry_set_perm(archive_entry_ptr, 0644);
            archive_entry_set_size(archive_entry_ptr, static_cast<la_int64_t>(contents.size()));
            REQUIRE(archive_write_header(writer, archive_entry_ptr) == ARCHIVE_OK);
            if (!contents.empty()) {
                REQUIRE(archive_write_data(writer, contents.data(), contents.size()) == static_cast<la_ssize_t>(contents.size()));
            }
        }

        REQUIRE(archive_write_finish_entry(writer) == ARCHIVE_OK);
        archive_entry_free(archive_entry_ptr);
    }

    archive_write_close(writer);
    archive_write_free(writer);
}

void WriteDuplicateEntryZip(const fs::path& archive_path) {
    struct archive* writer = archive_write_new();
    REQUIRE(writer != nullptr);
    REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
    REQUIRE(archive_write_open_filename(writer, archive_path.string().c_str()) == ARCHIVE_OK);

    for (int index = 0; index < 2; ++index) {
        struct archive_entry* archive_entry_ptr = archive_entry_new();
        REQUIRE(archive_entry_ptr != nullptr);
        const std::string contents = index == 0 ? "one" : "two";
        archive_entry_set_pathname(archive_entry_ptr, "manifest.yaml");
        archive_entry_set_filetype(archive_entry_ptr, AE_IFREG);
        archive_entry_set_perm(archive_entry_ptr, 0644);
        archive_entry_set_size(archive_entry_ptr, static_cast<la_int64_t>(contents.size()));
        REQUIRE(archive_write_header(writer, archive_entry_ptr) == ARCHIVE_OK);
        REQUIRE(archive_write_data(writer, contents.data(), contents.size()) == static_cast<la_ssize_t>(contents.size()));
        REQUIRE(archive_write_finish_entry(writer) == ARCHIVE_OK);
        archive_entry_free(archive_entry_ptr);
    }

    archive_write_close(writer);
    archive_write_free(writer);
}

void WriteCollisionZip(const fs::path& archive_path, std::string_view first, std::string_view second) {
    struct archive* writer = archive_write_new();
    REQUIRE(writer != nullptr);
    REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
    REQUIRE(archive_write_open_filename(writer, archive_path.string().c_str()) == ARCHIVE_OK);
    for (const std::string_view pathname : {first, second}) {
        struct archive_entry* entry = archive_entry_new();
        REQUIRE(entry != nullptr);
        archive_entry_set_pathname(entry, std::string(pathname).c_str());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, 1);
        REQUIRE(archive_write_header(writer, entry) == ARCHIVE_OK);
        REQUIRE(archive_write_data(writer, "x", 1) == 1);
        REQUIRE(archive_write_finish_entry(writer) == ARCHIVE_OK);
        archive_entry_free(entry);
    }
    archive_write_close(writer);
    archive_write_free(writer);
}

void WriteSingleEntryZip(const fs::path& archive_path, std::string_view path, unsigned int type, std::string_view target = {}) {
    struct archive* writer = archive_write_new();
    REQUIRE(writer != nullptr);
    REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
    REQUIRE(archive_write_open_filename(writer, archive_path.string().c_str()) == ARCHIVE_OK);

    struct archive_entry* archive_entry_ptr = archive_entry_new();
    REQUIRE(archive_entry_ptr != nullptr);
    archive_entry_set_pathname(archive_entry_ptr, std::string(path).c_str());
    archive_entry_set_filetype(archive_entry_ptr, type);
    archive_entry_set_perm(archive_entry_ptr, 0644);
    if (!target.empty()) {
        if (type == AE_IFLNK) {
            archive_entry_set_symlink(archive_entry_ptr, std::string(target).c_str());
        } else {
            archive_entry_set_hardlink(archive_entry_ptr, std::string(target).c_str());
        }
    }
    if (type == AE_IFREG) {
        static constexpr std::string_view kContents = "hello";
        archive_entry_set_size(archive_entry_ptr, static_cast<la_int64_t>(kContents.size()));
        REQUIRE(archive_write_header(writer, archive_entry_ptr) == ARCHIVE_OK);
        REQUIRE(archive_write_data(writer, kContents.data(), kContents.size()) == static_cast<la_ssize_t>(kContents.size()));
    } else {
        archive_entry_set_size(archive_entry_ptr, 0);
        REQUIRE(archive_write_header(writer, archive_entry_ptr) == ARCHIVE_OK);
    }

    REQUIRE(archive_write_finish_entry(writer) == ARCHIVE_OK);
    archive_entry_free(archive_entry_ptr);
    archive_write_close(writer);
    archive_write_free(writer);
}

#endif

TEST_CASE("Extension package installer installs a .wokiext zip archive") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive");
    const fs::path source = root / "source";
    fs::create_directories(source / "assets");

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(source / "extension.wasm", "");
    WriteFile(source / "assets" / "icon.txt", "icon");

    const fs::path archive_path = root / "hello.wokiext";
    WriteZipPackage(archive_path, source);

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE(installed.has_value());
    REQUIRE(installed->install_root == fs::weakly_canonical(roots.extensions / "woki.hello"));
    REQUIRE(fs::is_regular_file(installed->manifest));
    REQUIRE(fs::is_regular_file(installed->wasm));
    REQUIRE(fs::is_regular_file(installed->install_root / "assets" / "icon.txt"));
#else
    SKIP("Archive installation is native-only");
#endif
}

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST_CASE("Extension package installer opens Unicode archive paths on Windows") {
    const fs::path root = MakeTempDir("install_unicode_archive");
    const fs::path source = root / "source";
    fs::create_directories(source);
    WriteFile(source / "manifest.yaml", "id: woki.hello\nname: Hello\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n");
    WriteFile(source / "extension.wasm", "module");
    const fs::path archive_path = root / fs::path{L"hello-\u03bb.wokiext"};
    WriteZipPackage(archive_path, source);
    const woki::ext::Roots roots{root / "extensions", root / "data", root / "cache", root / "config"};

    const auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE(installed);
    REQUIRE(fs::is_regular_file(installed->wasm));
}
#endif

TEST_CASE("Extension package installer rejects zip archives with unsupported entries") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive_unsupported");
    const fs::path source = root / "source";
    fs::create_directories(source);

    WriteFile(source / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(source / "extension.wasm", "");
    WriteFile(source / "random.txt", "bad");

    const fs::path archive_path = root / "bad.wokiext";
    WriteZipPackage(archive_path, source);

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(installed.error().Message().contains("unsupported"));
#else
    SKIP("Archive installation is native-only");
#endif
}

TEST_CASE("Extension package installer rejects duplicate archive entries") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive_duplicate");
    const fs::path archive_path = root / "duplicate.wokiext";
    WriteDuplicateEntryZip(archive_path);

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(installed.error().Message().contains("duplicate"));
#else
    SKIP("Archive installation is native-only");
#endif
}

TEST_CASE("Extension package installer rejects portable archive path collisions") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive_collisions");
    const woki::ext::Roots roots{root / "extensions", root / "ext-data", root / "cache" / "ext"};

    const fs::path case_archive = root / "case.wokiext";
    WriteCollisionZip(case_archive, "assets/Icon.txt", "assets/icon.txt");
    auto case_result = woki::ext::InstallArchive(case_archive, roots);
    REQUIRE_FALSE(case_result.has_value());
    REQUIRE(case_result.error().Message().contains("collision"));

    const fs::path hierarchy_archive = root / "hierarchy.wokiext";
    WriteCollisionZip(hierarchy_archive, "assets", "assets/icon.txt");
    auto hierarchy_result = woki::ext::InstallArchive(hierarchy_archive, roots);
    REQUIRE_FALSE(hierarchy_result.has_value());
    REQUIRE(hierarchy_result.error().Message().contains("collision"));
#else
    SKIP("Archive installation is native-only");
#endif
}

TEST_CASE("Extension package installer rejects archive path traversal") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive_traversal");
    const fs::path archive_path = root / "traversal.wokiext";
    WriteSingleEntryZip(archive_path, "../manifest.yaml", AE_IFREG);

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(installed.error().Message().contains(".."));
#else
    SKIP("Archive installation is native-only");
#endif
}

TEST_CASE("Extension package installer rejects archive links") {
#ifndef __EMSCRIPTEN__
    const fs::path root = MakeTempDir("install_archive_link");
    const fs::path archive_path = root / "link.wokiext";
    WriteSingleEntryZip(archive_path, "assets/link", AE_IFLNK, "manifest.yaml");

    const woki::ext::Roots roots{
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    };

    auto installed = woki::ext::InstallArchive(archive_path, roots);
    REQUIRE_FALSE(installed.has_value());
    REQUIRE(installed.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(installed.error().Message().contains("link"));
#else
    SKIP("Archive installation is native-only");
#endif
}
