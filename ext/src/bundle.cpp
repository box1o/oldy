#include <array>
#include <random>
#include <string>
#include <fstream>
#include <algorithm>
#include <archive.h>
#include <filesystem>
#include <archive_entry.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <woki/ext/package.hpp>
#include <woki/ext/manifest.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

inline constexpr std::uintmax_t kMaxSingleFileBytes = 64u * 1024u * 1024u;
inline constexpr std::uintmax_t kMaxTotalPackageBytes = 256u * 1024u * 1024u;
inline constexpr std::size_t kMaxPackageEntries = 10'000u;

[[nodiscard]] fs::path DefaultBundlePath(const fs::path& project, const fs::path& package) {
    auto manifest = woki::ext::LoadManifest(package / "manifest.yaml");
    if (!manifest) {
        return project.parent_path() / (project.filename().string() + ".wokiext");
    }
    return project.parent_path() / (manifest->id + "-" + manifest->version + ".wokiext");
}

[[nodiscard]] bool FilesEqual(const fs::path& left, const fs::path& right) {
    std::error_code error;
    const std::uintmax_t left_size = fs::file_size(left, error);
    if (error) {
        return false;
    }
    const std::uintmax_t right_size = fs::file_size(right, error);
    if (error || left_size != right_size) {
        return false;
    }
    std::ifstream lhs(left, std::ios::binary);
    std::ifstream rhs(right, std::ios::binary);
    std::array<char, 64 * 1024> left_bytes{};
    std::array<char, 64 * 1024> right_bytes{};
    do {
        lhs.read(left_bytes.data(), static_cast<std::streamsize>(left_bytes.size()));
        rhs.read(right_bytes.data(), static_cast<std::streamsize>(right_bytes.size()));
        if (lhs.gcount() != rhs.gcount() || !std::equal(left_bytes.begin(), left_bytes.begin() + static_cast<std::ptrdiff_t>(lhs.gcount()), right_bytes.begin())) {
            return false;
        }
    } while (lhs.gcount() != 0);
    return !lhs.bad() && !rhs.bad();
}

[[nodiscard]] bool IsWithin(const fs::path& child, const fs::path& parent) {
    auto child_it = child.begin();
    for (auto parent_it = parent.begin(); parent_it != parent.end(); ++parent_it, ++child_it) {
        if (child_it == child.end() || *child_it != *parent_it)
            return false;
    }
    return true;
}

[[nodiscard]] bool ContainsSymlink(const fs::path& path) {
    fs::path current;
    std::error_code error;
    for (const auto& component : path) {
        current /= component;
        if (fs::is_symlink(fs::symlink_status(current, error)) || error)
            return true;
    }
    return false;
}

[[nodiscard]] std::string PathArgument(const fs::path& path) {
#ifdef _WIN32
    const std::u8string value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
    return path.string();
#endif
}

[[nodiscard]] std::expected<fs::path, std::string> BundleRoot(Context& context, const BundleOptions& options, const fs::path& requested) {
    if (!fs::is_regular_file(requested / "CMakeLists.txt")) {
        return requested;
    }

    const fs::path package = requested / "build/package";
    const fs::path state = requested / "build/package.state";
    if (!fs::is_regular_file(package / "manifest.yaml") || !fs::is_regular_file(state)) {
        return std::unexpected("Extension project has no staged package; run wokiext build first");
    }
    if (!FilesEqual(requested / "manifest.yaml", package / "manifest.yaml")) {
        return std::unexpected("Staged extension package is stale; rebuild after changing manifest.yaml");
    }

    const fs::path module_dir = FindCMakeModuleDir(options.executable);
    const fs::path sdk_dir = FindSdkDir(options.executable);
    if (module_dir.empty() || sdk_dir.empty())
        return std::unexpected("Cannot locate extension tooling needed to validate staged package content");
    const std::vector<std::string> check_arguments{"cmake", "-D", "PROJECT_DIR=" + PathArgument(requested), "-D", "SDK_DIR=" + PathArgument(sdk_dir), "-D", "STATE_FILE=" + PathArgument(state), "-P",
        PathArgument(module_dir / "CheckExtensionState.cmake")};
    if (!context.processes.Run(check_arguments, context.diagnostics))
        return std::unexpected("Staged extension package content does not match the project; run wokiext build first");
    return package;
}

[[nodiscard]] fs::path TemporarySibling(const fs::path& output) {
    std::random_device random;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const fs::path candidate = output.parent_path() / (output.filename().string() + ".tmp-" + std::to_string(random()));
        std::error_code error;
        if (!fs::exists(candidate, error) && !error)
            return candidate;
    }
    return {};
}

[[nodiscard]] bool ReplaceFile(const fs::path& temporary, const fs::path& output, std::error_code& error) {
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), output.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
        return true;
    error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
    return false;
#else
    fs::rename(temporary, output, error);
    return !error;
#endif
}

[[nodiscard]] Status WriteEntry(Context& context, struct archive* writer, const fs::path& root, const fs::path& path, const std::filesystem::path& wasm_path, std::uintmax_t& total_bytes) {
    const fs::path relative = fs::relative(path, root);
    if (!woki::ext::IsAllowedArchiveEntry(relative, wasm_path)) {
        return Status::Ok;
    }

    std::error_code error;
    if (fs::is_symlink(path, error) || !fs::is_regular_file(path, error)) {
        context.diagnostics.Err() << "Bundle can only include regular files: " << relative << '\n';
        return Status::Error;
    }

    const std::uintmax_t file_size = fs::file_size(path, error);
    if (error || file_size > kMaxSingleFileBytes || file_size > kMaxTotalPackageBytes - total_bytes) {
        context.diagnostics.Err() << "Bundle entry exceeds package size limits: " << relative << '\n';
        return Status::Error;
    }
    total_bytes += file_size;

    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        context.diagnostics.Err() << "Failed to read bundle entry: " << path << '\n';
        return Status::Error;
    }
    struct archive_entry* entry = archive_entry_new();
    if (entry == nullptr) {
        context.diagnostics.Error("Failed to allocate archive entry");
        return Status::Error;
    }

    const std::u8string relative_utf8 = relative.generic_u8string();
    const std::string archive_path(reinterpret_cast<const char*>(relative_utf8.data()), relative_utf8.size());
    archive_entry_set_pathname_utf8(entry, archive_path.c_str());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, static_cast<la_int64_t>(file_size));

    if (archive_write_header(writer, entry) != ARCHIVE_OK) {
        context.diagnostics.Error(archive_error_string(writer));
        archive_entry_free(entry);
        return Status::Error;
    }
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && archive_write_data(writer, buffer.data(), static_cast<std::size_t>(count)) != count) {
            context.diagnostics.Error(archive_error_string(writer));
            archive_entry_free(entry);
            return Status::Error;
        }
    }
    if (input.bad()) {
        context.diagnostics.Err() << "Failed to read bundle entry: " << path << '\n';
        archive_entry_free(entry);
        return Status::Error;
    }
    archive_entry_free(entry);
    return Status::Ok;
}

} // namespace

Status Bundle(Context& context, const BundleOptions& options) {
    const fs::path requested = fs::absolute(options.path).lexically_normal();
    std::error_code path_error;
    const fs::path canonical_requested = fs::canonical(requested, path_error);
    if (path_error || canonical_requested != requested || ContainsSymlink(requested)) {
        context.diagnostics.Error("Bundle input must be an existing canonical path without symbolic links");
        return Status::Error;
    }
    auto resolved_root = BundleRoot(context, options, requested);
    if (!resolved_root) {
        context.diagnostics.Error(resolved_root.error());
        return Status::Error;
    }
    const fs::path root = *resolved_root;
    const fs::path canonical_root = fs::canonical(root, path_error);
    if (path_error || canonical_root != root || ContainsSymlink(root)) {
        context.diagnostics.Error("Bundle package must be a canonical path without symbolic links");
        return Status::Error;
    }
    PathOptions verify{.path = root};
    if (Verify(context, verify) != Status::Ok) {
        return Status::Error;
    }

    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        context.diagnostics.Error(manifest.error().Message());
        return Status::Error;
    }

    const fs::path out_file = (options.out_file.empty() ? DefaultBundlePath(requested, root) : fs::absolute(options.out_file)).lexically_normal();
    std::error_code directory_error;
    fs::create_directories(out_file.parent_path(), directory_error);
    if (directory_error) {
        context.diagnostics.Error("Cannot create bundle output directory: " + directory_error.message());
        return Status::Error;
    }
    const fs::path canonical_output_parent = fs::canonical(out_file.parent_path(), path_error);
    const fs::path canonical_output = canonical_output_parent / out_file.filename();
    if (path_error || ContainsSymlink(out_file.parent_path()) || fs::is_symlink(fs::symlink_status(out_file, path_error)) || IsWithin(canonical_output, canonical_root)
        || IsWithin(canonical_output, canonical_requested)) {
        context.diagnostics.Error("Bundle output must be a canonical non-symbolic path outside the project and package directories");
        return Status::Error;
    }
    const fs::path temporary = TemporarySibling(out_file);
    if (temporary.empty()) {
        context.diagnostics.Error("Cannot allocate a temporary bundle output");
        return Status::Error;
    }

    struct archive* writer = archive_write_new();
    if (writer == nullptr) {
        context.diagnostics.Error("Failed to allocate archive writer");
        return Status::Error;
    }

    if (archive_write_set_format_zip(writer) != ARCHIVE_OK) {
        context.diagnostics.Error(archive_error_string(writer));
        archive_write_free(writer);
        return Status::Error;
    }
#ifdef _WIN32
    const int open_status = archive_write_open_filename_w(writer, temporary.c_str());
#else
    const int open_status = archive_write_open_filename(writer, temporary.c_str());
#endif
    if (open_status != ARCHIVE_OK) {
        context.diagnostics.Error(archive_error_string(writer));
        archive_write_free(writer);
        fs::remove(temporary, directory_error);
        return Status::Error;
    }

    Status status = Status::Ok;
    std::size_t entry_count = 0;
    std::uintmax_t total_bytes = 0;
    std::error_code iteration_error;
    fs::recursive_directory_iterator iterator(root, fs::directory_options::none, iteration_error);
    const fs::recursive_directory_iterator end;
    for (; iterator != end && !iteration_error; iterator.increment(iteration_error)) {
        const fs::directory_entry& entry = *iterator;
        if (entry.is_symlink(iteration_error)) {
            context.diagnostics.Err() << "Bundle input contains a symbolic link: " << entry.path() << '\n';
            status = Status::Error;
            break;
        }
        if (entry.is_directory()) {
            continue;
        }
        if (++entry_count > kMaxPackageEntries) {
            context.diagnostics.Error("Bundle exceeds 10000 entry limit");
            status = Status::Error;
            break;
        }
        status = WriteEntry(context, writer, root, entry.path(), manifest->wasm_path, total_bytes);
        if (status != Status::Ok) {
            break;
        }
    }
    if (iteration_error && status == Status::Ok) {
        context.diagnostics.Error("Cannot inspect bundle input: " + iteration_error.message());
        status = Status::Error;
    }

    if (archive_write_close(writer) != ARCHIVE_OK) {
        context.diagnostics.Error(archive_error_string(writer));
        status = Status::Error;
    }
    if (archive_write_free(writer) != ARCHIVE_OK) {
        status = Status::Error;
    }

    if (status != Status::Ok) {
        std::error_code error;
        fs::remove(temporary, error);
        return status;
    }

    std::error_code replace_error;
    if (!ReplaceFile(temporary, out_file, replace_error)) {
        fs::remove(temporary, directory_error);
        context.diagnostics.Error("Cannot activate bundle output: " + replace_error.message());
        return Status::Error;
    }

    context.diagnostics.Out() << "Bundled extension: " << out_file << '\n';
    return Status::Ok;
}

} // namespace wokiext
