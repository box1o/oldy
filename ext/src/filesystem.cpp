#include <cerrno>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <array>
#include <random>
#include <iomanip>
#include <sstream>
#else
#include <cstdlib>
#endif

#include "cli_internal.hpp"

namespace wokiext {

bool SystemFilesystem::Exists(const std::filesystem::path& path, std::error_code& error) const {
    return std::filesystem::exists(path, error);
}

bool SystemFilesystem::IsRegularFile(const std::filesystem::path& path) const {
    return std::filesystem::is_regular_file(path);
}

bool SystemFilesystem::IsDirectory(const std::filesystem::path& path) const {
    return std::filesystem::is_directory(path);
}

bool SystemFilesystem::IsSymlink(const std::filesystem::path& path, std::error_code& error) const {
    return std::filesystem::is_symlink(std::filesystem::symlink_status(path, error));
}

bool SystemFilesystem::CreateDirectories(const std::filesystem::path& path, std::error_code& error) {
    return std::filesystem::create_directories(path, error);
}

std::uintmax_t SystemFilesystem::RemoveAll(const std::filesystem::path& path, std::error_code& error) {
    return std::filesystem::remove_all(path, error);
}

bool SystemFilesystem::Remove(const std::filesystem::path& path, std::error_code& error) {
    return std::filesystem::remove(path, error);
}

bool SystemFilesystem::CopyFile(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error) {
    return std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
}

std::filesystem::path SystemFilesystem::CurrentPath(std::error_code& error) const {
    return std::filesystem::current_path(error);
}

std::filesystem::path SystemFilesystem::Absolute(const std::filesystem::path& path) const {
    return std::filesystem::absolute(path).lexically_normal();
}

TemporaryDirectory::TemporaryDirectory(TemporaryDirectory&& other) noexcept
    : path_(std::move(other.path_)) {
    other.path_.clear();
}

TemporaryDirectory& TemporaryDirectory::operator=(TemporaryDirectory&& other) noexcept {
    if (this != &other) {
        std::error_code error;
        if (!path_.empty())
            std::filesystem::remove_all(path_, error);
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

TemporaryDirectory::~TemporaryDirectory() {
    std::error_code error;
    if (!path_.empty())
        std::filesystem::remove_all(path_, error);
}

std::expected<TemporaryDirectory, std::string> TemporaryDirectory::Create(std::string_view prefix) {
    std::error_code error;
    const std::filesystem::path base = std::filesystem::temp_directory_path(error);
    if (error)
        return std::unexpected("Cannot locate the temporary directory: " + error.message());

#ifndef _WIN32
    std::string pattern = (base / (std::string(prefix) + "XXXXXX")).string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (char* created = ::mkdtemp(buffer.data()))
        return TemporaryDirectory(std::filesystem::path(created));
    return std::unexpected("Cannot create temporary directory: " + std::string(std::strerror(errno)));
#else
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt) {
        std::array<std::uint32_t, 4> words{random(), random(), random(), random()};
        std::ostringstream token;
        token << std::hex << std::setfill('0');
        for (const auto word : words)
            token << std::setw(8) << word;
        const std::filesystem::path candidate = base / (std::string(prefix) + token.str());
        if (std::filesystem::create_directory(candidate, error)) {
            std::filesystem::permissions(candidate, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
            if (!error)
                return TemporaryDirectory(candidate);
            std::error_code cleanup_error;
            std::filesystem::remove(candidate, cleanup_error);
            return std::unexpected("Cannot secure temporary directory: " + error.message());
        }
        if (error && error != std::errc::file_exists)
            return std::unexpected("Cannot create temporary directory: " + error.message());
        error.clear();
    }
    return std::unexpected("Cannot allocate a unique temporary directory");
#endif
}

} // namespace wokiext
