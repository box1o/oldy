#ifndef __EMSCRIPTEN__
#include <archive.h>
#include <archive_entry.h>
#endif

#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

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
    output << contents;
}

[[nodiscard]] woki::ext::Manifest MakeManifest() {
    woki::ext::Manifest manifest;
    manifest.id = "woki.hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.permissions = {woki::ext::Permission::Log};
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
    REQUIRE(layout->cache_root == fs::weakly_canonical(root / "cache" / "woki" / "ext" / manifest.id));
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

#ifndef __EMSCRIPTEN__

void WriteZipPackage(const fs::path& archive_path, const fs::path& source_root) {
    struct archive* writer = archive_write_new();
    REQUIRE(writer != nullptr);
    REQUIRE(archive_write_set_format_zip(writer) == ARCHIVE_OK);
    REQUIRE(archive_write_open_filename(writer, archive_path.string().c_str()) == ARCHIVE_OK);

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

void WriteSingleEntryZip(const fs::path& archive_path, std::string_view path, mode_t type, std::string_view target = {}) {
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
