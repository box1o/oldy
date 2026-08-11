#pragma once

#include <span>
#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <variant>
#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include "wokiext/cli.hpp"

namespace wokiext {

enum class Status : int { Ok = 0, Error = 1, Usage = 2 };

struct CreateOptions {
    std::string name;
    std::string id;
    std::filesystem::path out_dir;
    std::string lang{"cpp"};
    std::filesystem::path executable;
};

using TemplateReplacements = std::unordered_map<std::string, std::string>;

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
    std::filesystem::path executable;
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

struct HelpCommand {};

struct CreateCommand {
    CreateOptions options;
};

struct BuildCommand {
    BuildOptions options;
};

struct VerifyCommand {
    PathOptions options;
};

struct BundleCommand {
    BundleOptions options;
};

struct InstallCommand {
    InstallOptions options;
};

struct ListCommand {
    ListOptions options;
};

struct RemoveCommand {
    RemoveOptions options;
};

struct CommandsCommand {
    CommandsOptions options;
};

struct SchemaCommand {};

struct RunCommand {
    BuildOptions options;
};

struct TestCommand {
    BuildOptions options;
};

struct CleanCommand {
    PathOptions options;
};

using Command = std::variant<HelpCommand, CreateCommand, BuildCommand, VerifyCommand, BundleCommand, InstallCommand, ListCommand, RemoveCommand, CommandsCommand, SchemaCommand, RunCommand, TestCommand, CleanCommand>;

struct ParseError {
    std::string message;
};

class Diagnostics {
public:
    Diagnostics(std::ostream& output, std::ostream& error);
    [[nodiscard]] std::ostream& Out() const;
    [[nodiscard]] std::ostream& Err() const;
    void Info(std::string_view message) const;
    void Error(std::string_view message) const;
    void Warning(std::string_view message) const;

private:
    std::ostream* output_;
    std::ostream* error_;
};

class ProcessRunner {
public:
    virtual ~ProcessRunner() = default;
    [[nodiscard]] virtual bool Run(std::span<const std::string> arguments, Diagnostics& diagnostics) = 0;
};

class SystemProcessRunner final : public ProcessRunner {
public:
    [[nodiscard]] bool Run(std::span<const std::string> arguments, Diagnostics& diagnostics) override;
};

class Filesystem {
public:
    virtual ~Filesystem() = default;
    [[nodiscard]] virtual bool Exists(const std::filesystem::path& path, std::error_code& error) const = 0;
    [[nodiscard]] virtual bool IsRegularFile(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual bool IsDirectory(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual bool IsSymlink(const std::filesystem::path& path, std::error_code& error) const = 0;
    virtual bool CreateDirectories(const std::filesystem::path& path, std::error_code& error) = 0;
    virtual std::uintmax_t RemoveAll(const std::filesystem::path& path, std::error_code& error) = 0;
    virtual bool Remove(const std::filesystem::path& path, std::error_code& error) = 0;
    virtual bool CopyFile(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error) = 0;
    [[nodiscard]] virtual std::filesystem::path CurrentPath(std::error_code& error) const = 0;
    [[nodiscard]] virtual std::filesystem::path Absolute(const std::filesystem::path& path) const = 0;
};

class SystemFilesystem final : public Filesystem {
public:
    [[nodiscard]] bool Exists(const std::filesystem::path& path, std::error_code& error) const override;
    [[nodiscard]] bool IsRegularFile(const std::filesystem::path& path) const override;
    [[nodiscard]] bool IsDirectory(const std::filesystem::path& path) const override;
    [[nodiscard]] bool IsSymlink(const std::filesystem::path& path, std::error_code& error) const override;
    bool CreateDirectories(const std::filesystem::path& path, std::error_code& error) override;
    std::uintmax_t RemoveAll(const std::filesystem::path& path, std::error_code& error) override;
    bool Remove(const std::filesystem::path& path, std::error_code& error) override;
    bool CopyFile(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error) override;
    [[nodiscard]] std::filesystem::path CurrentPath(std::error_code& error) const override;
    [[nodiscard]] std::filesystem::path Absolute(const std::filesystem::path& path) const override;
};

class TemporaryDirectory {
public:
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&& other) noexcept;
    TemporaryDirectory& operator=(TemporaryDirectory&& other) noexcept;
    ~TemporaryDirectory();
    [[nodiscard]] static std::expected<TemporaryDirectory, std::string> Create(std::string_view prefix);

    [[nodiscard]] const std::filesystem::path& Path() const {
        return path_;
    }

private:
    explicit TemporaryDirectory(std::filesystem::path path)
        : path_(std::move(path)) {}

    std::filesystem::path path_;
};

struct Context {
    Diagnostics& diagnostics;
    ProcessRunner& processes;
    Filesystem& filesystem;
};

[[nodiscard]] std::expected<Command, ParseError> ParseCommand(std::span<const std::string_view> args, const std::filesystem::path& current_directory);
[[nodiscard]] Status Dispatch(Context& context, const Command& command);
[[nodiscard]] int Run(Context& context, std::span<const char* const> args);
void PrintUsage(Diagnostics& diagnostics, std::string_view executable);
[[nodiscard]] std::vector<std::string> BuildConfigureArguments(const BuildOptions& options, const std::filesystem::path& module_dir, const std::filesystem::path& sdk_dir);
[[nodiscard]] std::vector<std::string> BuildCompileArguments(const BuildOptions& options);
[[nodiscard]] std::filesystem::path BuiltPackagePath(const BuildOptions& options);
[[nodiscard]] std::filesystem::path FindCMakeModuleDir(const std::filesystem::path& executable);
[[nodiscard]] std::filesystem::path FindSdkDir(const std::filesystem::path& executable);
[[nodiscard]] std::filesystem::path FindTemplateDir(const std::filesystem::path& executable);
[[nodiscard]] std::expected<std::string, std::string> RenderTemplate(std::string_view contents, const TemplateReplacements& replacements);
[[nodiscard]] std::expected<void, std::string> InstantiateTemplates(const std::filesystem::path& template_dir, const std::filesystem::path& destination, const TemplateReplacements& replacements);
[[nodiscard]] Status Create(Context& context, const CreateOptions& options);
[[nodiscard]] Status Build(Context& context, const BuildOptions& options);
[[nodiscard]] Status Verify(Context& context, const PathOptions& options);
[[nodiscard]] Status Bundle(Context& context, const BundleOptions& options);
[[nodiscard]] Status Install(Context& context, const InstallOptions& options);
[[nodiscard]] Status List(Context& context, const ListOptions& options);
[[nodiscard]] Status Remove(Context& context, const RemoveOptions& options);
[[nodiscard]] Status Commands(Context& context, const CommandsOptions& options);
[[nodiscard]] Status Schema(Context& context);
[[nodiscard]] Status Clean(Context& context, const PathOptions& options);

} // namespace wokiext
