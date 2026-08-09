#include <string>
#include <iostream>
#include <cxxopts.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

[[nodiscard]] std::filesystem::path CurrentDirectory() {
    std::error_code error;
    auto path = std::filesystem::current_path(error);
    if (error) {
        return ".";
    }
    return path;
}

[[nodiscard]] std::string RequirePositional(const cxxopts::ParseResult& result, std::string_view name) {
    const std::string key{name};
    if (!result.count(key)) {
        throw cxxopts::exceptions::missing_argument(key);
    }
    return result[key].as<std::string>();
}

} // namespace

void PrintUsage(std::string_view executable) {
    std::cout << "Usage:\n"
              << "  " << executable << " create <name> [--id <id>] [--out <dir>] [--lang c|cpp]\n"
              << "  " << executable << " build <path> [--release|--debug]\n"
              << "  " << executable << " verify <path>\n"
              << "  " << executable << " bundle <path> [--out <file.wokiext>]\n"
              << "  " << executable << " install <path-or-wokiext> [--root <dir>] [--force]\n"
              << "  " << executable << " list [--root <dir>]\n"
              << "  " << executable << " remove <id> [--root <dir>] [--keep-data]\n"
              << "  " << executable << " commands [<path>] [--root <dir>] [--json]\n"
              << "  " << executable << " schema\n"
              << "  " << executable << " run <path> [--release|--debug]  (build, verify, bundle)\n"
              << "  " << executable << " test <path> [--release|--debug]  (build, verify)\n"
              << "  " << executable << " clean <path>\n";
}

int Run(std::span<const char* const> args) {
    const std::string executable = args.empty() ? "wokiext" : args.front();
    if (args.size() < 2) {
        PrintUsage(executable);
        return static_cast<int>(Status::Usage);
    }

    const std::string command = args[1];
    if (command == "-h" || command == "--help" || command == "help") {
        PrintUsage(executable);
        return static_cast<int>(Status::Ok);
    }

    std::vector<const char*> command_args;
    command_args.reserve(args.size() - 1);
    command_args.push_back(args.front());
    for (std::size_t i = 2; i < args.size(); ++i) {
        command_args.push_back(args[i]);
    }

    try {
        if (command == "create") {
            cxxopts::Options options(executable, "Create a Woki extension project");
            options.add_options()("id", "Manifest extension id", cxxopts::value<std::string>())("out", "Output parent directory", cxxopts::value<std::string>())("lang", "Template language: c or cpp",
                cxxopts::value<std::string>()->default_value("cpp"))("name", "Extension name", cxxopts::value<std::string>());
            options.parse_positional({"name"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            CreateOptions create{
                .name = RequirePositional(parsed, "name"),
                .id = parsed.count("id") ? parsed["id"].as<std::string>() : std::string{},
                .out_dir = parsed.count("out") ? std::filesystem::path(parsed["out"].as<std::string>()) : CurrentDirectory(),
                .lang = parsed["lang"].as<std::string>(),
            };
            return static_cast<int>(Create(create));
        }

        if (command == "build" || command == "run" || command == "test") {
            cxxopts::Options options(executable, "Build a Woki extension project");
            options.add_options()("release", "Build Release configuration")("debug", "Build Debug configuration")("path", "Extension project path", cxxopts::value<std::string>());
            options.parse_positional({"path"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            BuildOptions build{
                .path = RequirePositional(parsed, "path"),
                .executable = executable,
                .config = parsed.count("debug") ? "Debug" : "Release",
            };
            const Status built = Build(build);
            if (built != Status::Ok || command == "build") {
                return static_cast<int>(built);
            }
            const Status verified = Verify(PathOptions{.path = build.path});
            if (verified != Status::Ok || command == "test") {
                return static_cast<int>(verified);
            }
            return static_cast<int>(Bundle(BundleOptions{.path = build.path, .out_file = std::filesystem::path{}}));
        }

        if (command == "verify" || command == "clean") {
            cxxopts::Options options(executable, command + " a Woki extension project");
            options.add_options()("path", "Extension path", cxxopts::value<std::string>());
            options.parse_positional({"path"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            PathOptions path{.path = RequirePositional(parsed, "path")};
            if (command == "verify") {
                return static_cast<int>(Verify(path));
            }

            if (!std::filesystem::is_regular_file(path.path / "CMakeLists.txt") || !std::filesystem::is_regular_file(path.path / "manifest.yaml")) {
                std::cerr << "Clean expects an extension project directory: " << path.path << '\n';
                return static_cast<int>(Status::Error);
            }

            std::error_code error;
            std::filesystem::remove_all(path.path / "build", error);
            if (!error) {
                std::filesystem::remove(path.path / "extension.wasm", error);
            }
            if (error) {
                std::cerr << "Failed to clean extension project: " << error.message() << '\n';
                return static_cast<int>(Status::Error);
            }
            return static_cast<int>(Status::Ok);
        }

        if (command == "bundle") {
            cxxopts::Options options(executable, "Bundle a Woki extension project");
            options.add_options()("out", "Output .wokiext path", cxxopts::value<std::string>())("path", "Extension project path", cxxopts::value<std::string>());
            options.parse_positional({"path"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            BundleOptions bundle{
                .path = RequirePositional(parsed, "path"),
                .out_file = parsed.count("out") ? std::filesystem::path(parsed["out"].as<std::string>()) : std::filesystem::path{},
            };
            return static_cast<int>(Bundle(bundle));
        }

        if (command == "install") {
            cxxopts::Options options(executable, "Install a Woki extension package");
            options.add_options()("root", "Installation root override", cxxopts::value<std::string>())("force", "Remove an existing installed package with the same id first")("path",
                "Extension directory or .wokiext path", cxxopts::value<std::string>());
            options.parse_positional({"path"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            InstallOptions install{
                .path = RequirePositional(parsed, "path"),
                .root = parsed.count("root") ? std::filesystem::path(parsed["root"].as<std::string>()) : std::filesystem::path{},
                .force = parsed.count("force") != 0,
            };
            return static_cast<int>(Install(install));
        }

        if (command == "list") {
            cxxopts::Options options(executable, "List installed Woki extensions");
            options.add_options()("root", "Installation root override", cxxopts::value<std::string>());

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            ListOptions list{
                .root = parsed.count("root") ? std::filesystem::path(parsed["root"].as<std::string>()) : std::filesystem::path{},
            };
            return static_cast<int>(List(list));
        }

        if (command == "remove") {
            cxxopts::Options options(executable, "Remove an installed Woki extension");
            options.add_options()("root", "Installation root override", cxxopts::value<std::string>())("keep-data", "Keep extension data and cache")("id", "Extension id", cxxopts::value<std::string>());
            options.parse_positional({"id"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            RemoveOptions remove{
                .id = RequirePositional(parsed, "id"),
                .root = parsed.count("root") ? std::filesystem::path(parsed["root"].as<std::string>()) : std::filesystem::path{},
                .keep_data = parsed.count("keep-data") != 0,
            };
            return static_cast<int>(Remove(remove));
        }

        if (command == "commands") {
            cxxopts::Options options(executable, "Print extension command contributions");
            options.add_options()("root", "Installation root override", cxxopts::value<std::string>())("json", "Print JSON output")("path", "Extension project path", cxxopts::value<std::string>());
            options.parse_positional({"path"});

            auto parsed = options.parse(static_cast<int>(command_args.size()), command_args.data());
            CommandsOptions commands{
                .path = parsed.count("path") ? std::filesystem::path(parsed["path"].as<std::string>()) : std::filesystem::path{},
                .root = parsed.count("root") ? std::filesystem::path(parsed["root"].as<std::string>()) : std::filesystem::path{},
                .json = parsed.count("json") != 0,
            };
            return static_cast<int>(Commands(commands));
        }

        if (command == "schema") {
            return static_cast<int>(Schema());
        }

        std::cerr << "Unknown command: " << command << '\n';
        PrintUsage(executable);
        return static_cast<int>(Status::Usage);
    } catch (const cxxopts::exceptions::exception& error) {
        std::cerr << error.what() << '\n';
        PrintUsage(executable);
        return static_cast<int>(Status::Usage);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return static_cast<int>(Status::Error);
    }
}

} // namespace wokiext
