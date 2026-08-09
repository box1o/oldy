#include <string>
#include <fstream>
#include <iostream>
#include <iterator>
#include <archive.h>
#include <filesystem>
#include <archive_entry.h>

#include <woki/ext/ext.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path DefaultBundlePath(const fs::path& root) {
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        return root.parent_path() / (root.filename().string() + ".wokiext");
    }
    return root.parent_path() / (manifest->id + "-" + manifest->version + ".wokiext");
}

[[nodiscard]] Status WriteEntry(struct archive* writer, const fs::path& root, const fs::path& path, const std::filesystem::path& wasm_path) {
    const fs::path relative = fs::relative(path, root);
    if (!woki::ext::IsAllowedArchiveEntry(relative, wasm_path)) {
        return Status::Ok;
    }

    std::error_code error;
    if (fs::is_symlink(path, error) || !fs::is_regular_file(path, error)) {
        std::cerr << "Bundle can only include regular files: " << relative << '\n';
        return Status::Error;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        std::cerr << "Failed to read bundle entry: " << path << '\n';
        return Status::Error;
    }
    const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

    struct archive_entry* entry = archive_entry_new();
    if (entry == nullptr) {
        std::cerr << "Failed to allocate archive entry\n";
        return Status::Error;
    }

    archive_entry_set_pathname(entry, relative.generic_string().c_str());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, static_cast<la_int64_t>(bytes.size()));

    if (archive_write_header(writer, entry) != ARCHIVE_OK) {
        std::cerr << archive_error_string(writer) << '\n';
        archive_entry_free(entry);
        return Status::Error;
    }
    if (!bytes.empty() && archive_write_data(writer, bytes.data(), bytes.size()) != static_cast<la_ssize_t>(bytes.size())) {
        std::cerr << archive_error_string(writer) << '\n';
        archive_entry_free(entry);
        return Status::Error;
    }
    archive_entry_free(entry);
    return Status::Ok;
}

} // namespace

Status Bundle(const BundleOptions& options) {
    const fs::path root = fs::absolute(options.path).lexically_normal();
    PathOptions verify{.path = root};
    if (Verify(verify) != Status::Ok) {
        return Status::Error;
    }

    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        std::cerr << manifest.error().Message() << '\n';
        return Status::Error;
    }

    const fs::path out_file = options.out_file.empty() ? DefaultBundlePath(root) : fs::absolute(options.out_file);
    fs::create_directories(out_file.parent_path());

    struct archive* writer = archive_write_new();
    if (writer == nullptr) {
        std::cerr << "Failed to allocate archive writer\n";
        return Status::Error;
    }

    if (archive_write_set_format_zip(writer) != ARCHIVE_OK) {
        std::cerr << archive_error_string(writer) << '\n';
        archive_write_free(writer);
        return Status::Error;
    }
    if (archive_write_open_filename(writer, out_file.string().c_str()) != ARCHIVE_OK) {
        std::cerr << archive_error_string(writer) << '\n';
        archive_write_free(writer);
        return Status::Error;
    }

    Status status = Status::Ok;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (entry.is_directory()) {
            continue;
        }
        status = WriteEntry(writer, root, entry.path(), manifest->wasm_path);
        if (status != Status::Ok) {
            break;
        }
    }

    if (archive_write_close(writer) != ARCHIVE_OK) {
        std::cerr << archive_error_string(writer) << '\n';
        status = Status::Error;
    }
    if (archive_write_free(writer) != ARCHIVE_OK) {
        status = Status::Error;
    }

    if (status != Status::Ok) {
        std::error_code error;
        fs::remove(out_file, error);
        return status;
    }

    std::cout << "Bundled extension: " << out_file << '\n';
    return Status::Ok;
}

} // namespace wokiext
