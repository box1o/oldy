#pragma once

#include <span>
#include <string>
#include <filesystem>
#include <string_view>

namespace wokiext {

enum class Status : int {
    Ok = 0,
    Usage = 2,
    Error = 1,
};

struct CreateOptions {
    std::string name;
    std::string id;
    std::filesystem::path out_dir;
    std::string lang{"cpp"};
};

struct BuildOptions {
    std::filesystem::path path;
    std::filesystem::path executable;
    std::string config{"Release"};
};

struct PathOptions {
    std::filesystem::path path;
};

struct BundleOptions {
    std::filesystem::path path;
    std::filesystem::path out_file;
};

struct InstallOptions {
    std::filesystem::path path;
    std::filesystem::path root;
    bool force{false};
};

struct ListOptions {
    std::filesystem::path root;
};

struct RemoveOptions {
    std::string id;
    std::filesystem::path root;
    bool keep_data{false};
};

struct CommandsOptions {
    std::filesystem::path path;
    std::filesystem::path root;
    bool json{false};
};

[[nodiscard]] int Run(std::span<const char* const> args);

[[nodiscard]] Status Create(const CreateOptions& options);
[[nodiscard]] Status Build(const BuildOptions& options);
[[nodiscard]] Status Verify(const PathOptions& options);
[[nodiscard]] Status Bundle(const BundleOptions& options);
[[nodiscard]] Status Install(const InstallOptions& options);
[[nodiscard]] Status List(const ListOptions& options);
[[nodiscard]] Status Remove(const RemoveOptions& options);
[[nodiscard]] Status Commands(const CommandsOptions& options);
[[nodiscard]] Status Schema();

void PrintUsage(std::string_view executable);

} // namespace wokiext
