#include <set>
#include <atomic>
#include <chrono>
#include <string>
#include <fstream>
#include <filesystem>

#include "woki/ext/package.hpp"
#include "woki/ext/path_safety.hpp"

#ifndef __EMSCRIPTEN__
#include <archive.h>
#include <archive_entry.h>
#endif

namespace woki::ext {

namespace {

namespace fs = std::filesystem;

inline constexpr std::uintmax_t kMaxSingleFileBytes = 64u * 1024u * 1024u;
inline constexpr std::uintmax_t kMaxTotalPackageBytes = 256u * 1024u * 1024u;
inline constexpr std::uintmax_t kMaxWasmBytes = 32u * 1024u * 1024u;
inline constexpr std::size_t kMaxPackageEntries = 10'000u;

[[nodiscard]] fs::path UniqueStagingRoot(const Roots& roots, std::string_view name) {
    static std::atomic_uint64_t sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return roots.cache / "staging" / (std::string(name) + "." + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1)) + ".installing");
}

[[nodiscard]] Result<fs::path> Normalize(const fs::path& path) {
    std::error_code error;
    fs::path normalized = fs::weakly_canonical(path, error);
    if (error) {
        normalized = path.lexically_normal();
    }
    if (normalized.empty()) {
        return Err(ErrorCode::ValidationInvalidState, "Resolved package path is empty");
    }
    return Ok(std::move(normalized));
}

[[nodiscard]] Result<void> ValidateSourceEntry(const fs::directory_entry& entry, const fs::path& source_root, const Manifest& manifest, std::uintmax_t* total_bytes) {
    std::error_code error;
    const fs::path relative = fs::relative(entry.path(), source_root, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }

    if (!IsAllowedArchiveEntry(relative, manifest.wasm_path)) {
        return Err(ErrorCode::ValidationInvalidState, "Package contains unsupported entry '" + relative.string()
                                                          + "'. Allowed entries are manifest.yaml, runtime.wasm, assets/**, "
                                                            "extension.native/**, and signature.");
    }

    if (entry.is_symlink(error)) {
        return Err(ErrorCode::ValidationInvalidState, "Package entry must not be a symlink: " + relative.string());
    }
    if (entry.is_directory(error)) {
        return Ok();
    }
    if (!entry.is_regular_file(error)) {
        return Err(ErrorCode::ValidationInvalidState, "Package entry must be a regular file or directory: " + relative.string());
    }

    const auto size = entry.file_size(error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (size > kMaxSingleFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Package file exceeds 64 MiB limit: " + relative.string());
    }
    *total_bytes += size;
    if (*total_bytes > kMaxTotalPackageBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Package exceeds 256 MiB unpacked size limit.");
    }

    return Ok();
}

[[nodiscard]] Result<void> CopyPackageTree(const fs::path& source_root, const fs::path& destination_root, const Manifest& manifest) {
    std::uintmax_t total_bytes = 0;
    std::size_t entry_count = 0;
    std::error_code error;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root, fs::directory_options::none, error)) {
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        if (++entry_count > kMaxPackageEntries) {
            return Err(ErrorCode::ValidationOutOfRange, "Package exceeds 10000 entry limit.");
        }

        auto valid = ValidateSourceEntry(entry, source_root, manifest, &total_bytes);
        if (!valid) {
            return Err(valid.error());
        }

        const fs::path relative = fs::relative(entry.path(), source_root, error);
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }

        const fs::path destination = destination_root / relative;
        if (entry.is_directory(error)) {
            fs::create_directories(destination, error);
            if (error) {
                return Err(ErrorCode::FileWriteError, error.message());
            }
            continue;
        }

        fs::create_directories(destination.parent_path(), error);
        if (error) {
            return Err(ErrorCode::FileWriteError, error.message());
        }

        fs::copy_file(entry.path(), destination, fs::copy_options::none, error);
        if (error) {
            return Err(ErrorCode::FileWriteError, error.message());
        }
    }

    return Ok();
}

[[nodiscard]] Result<void> ValidateWasmSize(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (size > kMaxWasmBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension wasm module exceeds 32 MiB limit.");
    }
    return Ok();
}

[[nodiscard]] bool ContainsSymlink(const fs::path& root, const fs::path& path) {
    const fs::path relative = path.lexically_relative(root);
    if (!IsSafeRelativePath(relative)) {
        return true;
    }
    fs::path current = root;
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(current, error))) {
        return true;
    }
    for (const fs::path& part : relative) {
        current /= part;
        if (fs::is_symlink(fs::symlink_status(current, error))) {
            return true;
        }
    }
    return false;
}

#ifndef __EMSCRIPTEN__

[[nodiscard]] Result<fs::path> SanitizeArchiveEntryPath(std::string_view entry_path) {
    if (entry_path.empty()) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry path is empty");
    }

    fs::path relative{entry_path};
    if (relative.is_absolute()) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry must be relative: " + std::string(entry_path));
    }
    if (HasPathTraversal(relative)) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry must not contain '..': " + std::string(entry_path));
    }

    relative = relative.lexically_normal();
    if (relative.empty() || relative == "." || HasPathTraversal(relative)) {
        return Err(ErrorCode::ValidationInvalidState, "Archive entry path is invalid: " + std::string(entry_path));
    }

    return Ok(std::move(relative));
}

[[nodiscard]] Result<void> ExtractArchive(const fs::path& archive_path, const fs::path& destination_root) {
    std::error_code error;
    if (!fs::is_regular_file(archive_path, error)) {
        return Err(ErrorCode::FileNotFound, "Extension archive is not a regular file: " + archive_path.string());
    }

    struct archive* reader = archive_read_new();
    if (reader == nullptr) {
        return Err(ErrorCode::InvalidState, "Failed to allocate libarchive read handle");
    }

    archive_read_support_format_zip(reader);
    archive_read_support_filter_all(reader);

    if (archive_read_open_filename(reader, archive_path.string().c_str(), 10240) != ARCHIVE_OK) {
        const std::string message = archive_error_string(reader);
        archive_read_free(reader);
        return Err(ErrorCode::ParseInvalidFormat, "Failed to open extension archive '" + archive_path.string() + "': " + message);
    }

    fs::create_directories(destination_root, error);
    if (error) {
        archive_read_close(reader);
        archive_read_free(reader);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    std::set<std::string> entries;
    std::uintmax_t total_bytes = 0;
    std::uintmax_t extracted_bytes = 0;
    struct archive_entry* entry = nullptr;

    while (true) {
        const int status = archive_read_next_header(reader, &entry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            const std::string message = archive_error_string(reader);
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ParseInvalidFormat, "Failed to read extension archive header: " + message);
        }

        const char* pathname = archive_entry_pathname(entry);
        if (pathname == nullptr) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ParseInvalidFormat, "Archive entry is missing a path");
        }
        if (entries.size() >= kMaxPackageEntries) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 10000 entry limit.");
        }

        auto relative = SanitizeArchiveEntryPath(pathname);
        if (!relative) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(relative.error());
        }

        const std::string entry_key = relative->generic_string();
        if (!entries.insert(entry_key).second) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationInvalidState, "Archive contains duplicate entry: " + entry_key);
        }

        if (archive_entry_hardlink(entry) != nullptr || archive_entry_symlink(entry) != nullptr) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationInvalidState, "Archive entry must not be a link: " + relative->string());
        }

        const auto file_type = archive_entry_filetype(entry);
        const fs::path destination = destination_root / *relative;

        if (file_type == AE_IFDIR) {
            fs::create_directories(destination, error);
            if (error) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileWriteError, error.message());
            }
            continue;
        }

        if (file_type != AE_IFREG) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::ValidationInvalidState, "Archive entry must be a regular file or directory: " + relative->string());
        }

        {
            const la_int64_t entry_size = archive_entry_size(entry);
            if (entry_size < 0) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ParseInvalidFormat, "Archive entry has invalid size: " + relative->string());
            }

            const auto size = static_cast<std::uintmax_t>(entry_size);
            if (size > kMaxSingleFileBytes) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Archive file exceeds 64 MiB limit: " + relative->string());
            }

            total_bytes += size;
            if (total_bytes > kMaxTotalPackageBytes) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 256 MiB unpacked size limit.");
            }
        }

        fs::create_directories(destination.parent_path(), error);
        if (error) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::FileWriteError, error.message());
        }

        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output.good()) {
            archive_read_close(reader);
            archive_read_free(reader);
            return Err(ErrorCode::FileWriteError, "Failed to create extracted archive file: " + destination.string());
        }

        const void* buffer = nullptr;
        std::size_t buffer_size = 0;
        la_int64_t offset = 0;
        std::uintmax_t file_bytes = 0;
        while (true) {
            const int block_status = archive_read_data_block(reader, &buffer, &buffer_size, &offset);
            if (block_status == ARCHIVE_EOF) {
                break;
            }
            if (block_status != ARCHIVE_OK) {
                const std::string message = archive_error_string(reader);
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileReadError, "Failed to read archive entry '" + relative->string() + "': " + message);
            }

            if (offset < 0 || static_cast<std::uintmax_t>(offset) > kMaxSingleFileBytes || buffer_size > kMaxSingleFileBytes - static_cast<std::uintmax_t>(offset)) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::ValidationOutOfRange, "Extracted archive file exceeds 64 MiB limit: " + relative->string());
            }
            const std::uintmax_t block_end = static_cast<std::uintmax_t>(offset) + buffer_size;
            if (block_end > file_bytes) {
                const std::uintmax_t growth = block_end - file_bytes;
                if (growth > kMaxTotalPackageBytes - extracted_bytes) {
                    archive_read_close(reader);
                    archive_read_free(reader);
                    return Err(ErrorCode::ValidationOutOfRange, "Archive exceeds 256 MiB unpacked size limit.");
                }
                extracted_bytes += growth;
                file_bytes = block_end;
            }
            output.seekp(static_cast<std::streamoff>(offset));
            output.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(buffer_size));
            if (!output.good()) {
                archive_read_close(reader);
                archive_read_free(reader);
                return Err(ErrorCode::FileWriteError, "Failed to write extracted archive file: " + destination.string());
            }
        }
    }

    archive_read_close(reader);
    archive_read_free(reader);
    return Ok();
}

#endif // __EMSCRIPTEN__

} // namespace

Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const fs::path& extensions_root, const fs::path& data_root, const fs::path& cache_root) {
    auto valid = ValidateManifest(manifest);
    if (!valid) {
        return Err(valid.error());
    }
    if (extensions_root.empty() || data_root.empty() || cache_root.empty()) {
        return Err(ErrorCode::InvalidArgument, "Extension package roots must not be empty.");
    }

    PackageLayout layout;

    auto install_root = Normalize(extensions_root / manifest.id);
    if (!install_root) {
        return Err(install_root.error());
    }
    layout.install_root = std::move(*install_root);

    auto data = Normalize(data_root / manifest.id);
    if (!data) {
        return Err(data.error());
    }

    auto cache = Normalize(cache_root / manifest.id);
    if (!cache) {
        return Err(cache.error());
    }

    layout.manifest = layout.install_root / "manifest.yaml";
    layout.wasm = (layout.install_root / manifest.wasm_path).lexically_normal();
    layout.data_root = std::move(*data);
    layout.cache_root = std::move(*cache);

    return Ok(std::move(layout));
}

Result<void> ValidatePackageLayout(const PackageLayout& layout) {
    std::error_code error;
    if (ContainsSymlink(layout.install_root, layout.manifest) || ContainsSymlink(layout.install_root, layout.wasm)) {
        return Err(ErrorCode::FileAccessDenied, "Extension package paths must not be symbolic links.");
    }
    if (!fs::is_directory(layout.install_root, error)) {
        return Err(ErrorCode::FileNotFound, "Extension install directory is missing: " + layout.install_root.string() + ". Install or extract the package before scanning.");
    }
    if (!fs::is_regular_file(layout.manifest, error)) {
        return Err(ErrorCode::FileNotFound, "Extension manifest is missing: " + layout.manifest.string()
                                                + ". Add manifest.yaml with id, name, version, apiVersion, runtime.wasm, and "
                                                  "permissions.");
    }
    if (!fs::is_regular_file(layout.wasm, error)) {
        return Err(ErrorCode::FileNotFound, "Extension wasm module is missing: " + layout.wasm.string() + ". Build the extension wasm or set runtime.wasm to the correct relative path.");
    }
    return ValidateWasmSize(layout.wasm);
}

Result<PackageLayout> InstallUnpackedPackage(const fs::path& source_root, const Roots& roots) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(source_root, error))) {
        return Err(ErrorCode::FileAccessDenied, "Unpacked extension package source must not be a symbolic link: " + source_root.string());
    }
    if (!fs::is_directory(source_root, error)) {
        return Err(ErrorCode::FileNotFound, "Unpacked extension package source is not a directory: " + source_root.string());
    }

    auto manifest = LoadManifest(source_root / "manifest.yaml");
    if (!manifest) {
        return Err(manifest.error());
    }

    auto layout = ResolvePackageLayout(*manifest, roots.extensions, roots.data, roots.cache);
    if (!layout) {
        return Err(layout.error());
    }

    if (fs::exists(layout->install_root, error)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + manifest->id + "' is already installed. Refusing to overwrite without an update policy.");
    }

    auto source_layout = ResolvePackageLayout(*manifest, source_root.parent_path(), roots.data, roots.cache);
    if (!source_layout) {
        return Err(source_layout.error());
    }
    source_layout->install_root = source_root;
    source_layout->manifest = source_root / "manifest.yaml";
    source_layout->wasm = (source_root / manifest->wasm_path).lexically_normal();

    auto valid_source = ValidatePackageLayout(*source_layout);
    if (!valid_source) {
        return Err(valid_source.error());
    }

    const fs::path staging_root = UniqueStagingRoot(roots, manifest->id);
    fs::create_directories(staging_root.parent_path(), error);
    if (error) {
        return Err(ErrorCode::FileWriteError, error.message());
    }

    auto copied = CopyPackageTree(source_root, staging_root, *manifest);
    if (!copied) {
        fs::remove_all(staging_root, error);
        return Err(copied.error());
    }

    fs::create_directories(layout->install_root.parent_path(), error);
    if (error) {
        fs::remove_all(staging_root, error);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    fs::rename(staging_root, layout->install_root, error);
    if (error) {
        fs::remove_all(staging_root, error);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    auto valid_installed = ValidatePackageLayout(*layout);
    if (!valid_installed) {
        fs::remove_all(layout->install_root, error);
        return Err(valid_installed.error());
    }

    return Ok(std::move(*layout));
}

Result<PackageLayout> InstallArchive(const fs::path& archive_path, const Roots& roots) {
#ifndef __EMSCRIPTEN__
    std::error_code error;
    const fs::path staging_root = UniqueStagingRoot(roots, "archive");
    fs::create_directories(staging_root, error);
    if (error) {
        return Err(ErrorCode::FileWriteError, error.message());
    }

    auto extracted = ExtractArchive(archive_path, staging_root);
    if (!extracted) {
        fs::remove_all(staging_root, error);
        return Err(extracted.error());
    }

    auto manifest = LoadManifest(staging_root / "manifest.yaml");
    if (!manifest) {
        fs::remove_all(staging_root, error);
        return Err(manifest.error());
    }

    std::uintmax_t total_bytes = 0;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(staging_root, fs::directory_options::none, error)) {
        if (error) {
            fs::remove_all(staging_root, error);
            return Err(ErrorCode::FileReadError, error.message());
        }

        auto valid = ValidateSourceEntry(entry, staging_root, *manifest, &total_bytes);
        if (!valid) {
            fs::remove_all(staging_root, error);
            return Err(valid.error());
        }
    }

    auto layout = ResolvePackageLayout(*manifest, roots.extensions, roots.data, roots.cache);
    if (!layout) {
        fs::remove_all(staging_root, error);
        return Err(layout.error());
    }

    if (fs::exists(layout->install_root, error)) {
        fs::remove_all(staging_root, error);
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + manifest->id + "' is already installed. Refusing to overwrite without an update policy.");
    }

    auto valid_installed_layout = ValidatePackageLayout(PackageLayout{
        .install_root = staging_root,
        .manifest = staging_root / "manifest.yaml",
        .wasm = (staging_root / manifest->wasm_path).lexically_normal(),
        .data_root = layout->data_root,
        .cache_root = layout->cache_root,
    });
    if (!valid_installed_layout) {
        fs::remove_all(staging_root, error);
        return Err(valid_installed_layout.error());
    }

    fs::create_directories(layout->install_root.parent_path(), error);
    if (error) {
        fs::remove_all(staging_root, error);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    fs::rename(staging_root, layout->install_root, error);
    if (error) {
        fs::remove_all(staging_root, error);
        return Err(ErrorCode::FileWriteError, error.message());
    }

    auto valid_installed = ValidatePackageLayout(*layout);
    if (!valid_installed) {
        fs::remove_all(layout->install_root, error);
        return Err(valid_installed.error());
    }

    return Ok(std::move(*layout));
#else
    (void)archive_path;
    (void)roots;
    return Err(ErrorCode::InvalidState, "Extension archives are not supported on this platform.");
#endif
}

} // namespace woki::ext
