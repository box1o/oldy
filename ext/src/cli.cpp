#include <iostream>
#include <algorithm>
#include <type_traits>

#include "cli_internal.hpp"

namespace wokiext {
namespace {

struct ParsedArguments {
    std::vector<std::string_view> positional;
    std::vector<std::pair<std::string_view, std::string_view>> values;
    std::vector<std::string_view> flags;
};

[[nodiscard]] std::expected<ParsedArguments, ParseError> ParseArguments(std::span<const std::string_view> args, std::span<const std::string_view> value_options, std::span<const std::string_view> flag_options) {
    ParsedArguments parsed;
    bool options_ended = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view argument = args[i];
        if (!options_ended && argument == "--") {
            options_ended = true;
            continue;
        }
        if (options_ended || !argument.starts_with("--")) {
            parsed.positional.push_back(argument);
            continue;
        }

        argument.remove_prefix(2);
        const auto equals = argument.find('=');
        const std::string_view name = argument.substr(0, equals);
        const auto is_value = std::ranges::find(value_options, name) != value_options.end();
        const auto is_flag = std::ranges::find(flag_options, name) != flag_options.end();
        if (!is_value && !is_flag) {
            return std::unexpected(ParseError{"Unknown option: --" + std::string(name)});
        }
        if (is_flag) {
            if (equals != std::string_view::npos) {
                return std::unexpected(ParseError{"Option does not take a value: --" + std::string(name)});
            }
            if (std::ranges::find(parsed.flags, name) != parsed.flags.end()) {
                return std::unexpected(ParseError{"Option may be specified only once: --" + std::string(name)});
            }
            parsed.flags.push_back(name);
            continue;
        }

        std::string_view value;
        if (equals != std::string_view::npos) {
            value = argument.substr(equals + 1);
        } else if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
            ++i;
            value = args[i];
        } else {
            return std::unexpected(ParseError{"Option requires a value: --" + std::string(name)});
        }
        if (value.empty()) {
            return std::unexpected(ParseError{"Option requires a non-empty value: --" + std::string(name)});
        }
        if (std::ranges::any_of(parsed.values, [name](const auto& item) { return item.first == name; })) {
            return std::unexpected(ParseError{"Option may be specified only once: --" + std::string(name)});
        }
        parsed.values.emplace_back(name, value);
    }
    return parsed;
}

[[nodiscard]] bool HasFlag(const ParsedArguments& args, std::string_view name) {
    return std::ranges::find(args.flags, name) != args.flags.end();
}

[[nodiscard]] std::string_view Value(const ParsedArguments& args, std::string_view name) {
    for (auto it = args.values.rbegin(); it != args.values.rend(); ++it) {
        if (it->first == name)
            return it->second;
    }
    return {};
}

[[nodiscard]] bool HasValue(const ParsedArguments& args, std::string_view name) {
    return std::ranges::any_of(args.values, [name](const auto& value) { return value.first == name; });
}

[[nodiscard]] std::expected<std::string_view, ParseError> OnePositional(const ParsedArguments& args, std::string_view name) {
    if (args.positional.empty())
        return std::unexpected(ParseError{"Missing required argument: " + std::string(name)});
    if (args.positional.size() > 1)
        return std::unexpected(ParseError{"Unexpected argument: " + std::string(args.positional[1])});
    return args.positional.front();
}

[[nodiscard]] std::expected<BuildOptions, ParseError> MakeBuild(std::string_view path, std::string_view executable, const ParsedArguments& args) {
    const int selectors = static_cast<int>(HasFlag(args, "release")) + static_cast<int>(HasFlag(args, "debug")) + static_cast<int>(HasValue(args, "config"));
    if (selectors > 1) {
        return std::unexpected(ParseError{"Specify only one of --release, --debug, or --config"});
    }
    std::string config = "Release";
    if (HasFlag(args, "debug"))
        config = "Debug";
    else if (HasValue(args, "config"))
        config = Value(args, "config");
    return BuildOptions{.path = path, .executable = executable, .config = std::move(config)};
}

} // namespace

void PrintUsage(Diagnostics& diagnostics, std::string_view executable) {
    diagnostics.Out() << "Usage:\n"
                      << "  " << executable << " create <name> [--id <id>] [--out <dir>] [--lang c|cpp]\n"
                      << "  " << executable << " build <path> [--release|--debug|--config <name>]\n"
                      << "  " << executable << " verify <path>\n"
                      << "  " << executable << " bundle <path> [--out <file.wokiext>]\n"
                      << "  " << executable << " install <path-or-wokiext> [--root <dir>] [--force]\n"
                      << "  " << executable << " list [--root <dir>]\n"
                      << "  " << executable << " remove <id> [--root <dir>] [--keep-data]  (--keep-data preserves data, config, and cache)\n"
                      << "  " << executable << " commands [<path>] [--root <dir>] [--json]\n"
                      << "  " << executable << " schema\n"
                      << "  " << executable << " run <path> [--release|--debug|--config <name>]  (build, verify, bundle)\n"
                      << "  " << executable << " test <path> [--release|--debug|--config <name>]  (build, verify)\n"
                      << "  " << executable << " clean <path>\n";
}

std::expected<Command, ParseError> ParseCommand(std::span<const std::string_view> args, const std::filesystem::path& current_directory) {
    if (args.size() < 2)
        return std::unexpected(ParseError{});
    const std::string_view executable = args.front();
    const std::string_view command = args[1];
    const auto tail = args.subspan(2);
    if (command == "-h" || command == "--help" || command == "help")
        return HelpCommand{};

    if (command == "create") {
        constexpr std::string_view values[]{"id", "out", "lang"};
        auto parsed = ParseArguments(tail, values, {});
        if (!parsed)
            return std::unexpected(parsed.error());
        auto name = OnePositional(*parsed, "name");
        if (!name)
            return std::unexpected(name.error());
        return CreateCommand{{.name = std::string(*name),
            .id = std::string(Value(*parsed, "id")),
            .out_dir = Value(*parsed, "out").empty() ? current_directory : std::filesystem::path(Value(*parsed, "out")),
            .lang = Value(*parsed, "lang").empty() ? "cpp" : std::string(Value(*parsed, "lang"))}};
    }
    if (command == "build" || command == "run" || command == "test") {
        constexpr std::string_view values[]{"config"};
        constexpr std::string_view flags[]{"release", "debug"};
        auto parsed = ParseArguments(tail, values, flags);
        if (!parsed)
            return std::unexpected(parsed.error());
        auto path = OnePositional(*parsed, "path");
        if (!path)
            return std::unexpected(path.error());
        auto options = MakeBuild(*path, executable, *parsed);
        if (!options)
            return std::unexpected(options.error());
        if (command == "run")
            return RunCommand{std::move(*options)};
        if (command == "test")
            return TestCommand{std::move(*options)};
        return BuildCommand{std::move(*options)};
    }
    if (command == "verify" || command == "clean") {
        auto parsed = ParseArguments(tail, {}, {});
        if (!parsed)
            return std::unexpected(parsed.error());
        auto path = OnePositional(*parsed, "path");
        if (!path)
            return std::unexpected(path.error());
        if (command == "clean")
            return CleanCommand{{std::filesystem::path(*path)}};
        return VerifyCommand{{std::filesystem::path(*path)}};
    }
    if (command == "bundle") {
        constexpr std::string_view values[]{"out"};
        auto parsed = ParseArguments(tail, values, {});
        if (!parsed)
            return std::unexpected(parsed.error());
        auto path = OnePositional(*parsed, "path");
        if (!path)
            return std::unexpected(path.error());
        return BundleCommand{{std::filesystem::path(*path), std::filesystem::path(Value(*parsed, "out")), std::filesystem::path(executable)}};
    }
    if (command == "install") {
        constexpr std::string_view values[]{"root"};
        constexpr std::string_view flags[]{"force"};
        auto parsed = ParseArguments(tail, values, flags);
        if (!parsed)
            return std::unexpected(parsed.error());
        auto path = OnePositional(*parsed, "path");
        if (!path)
            return std::unexpected(path.error());
        return InstallCommand{{std::filesystem::path(*path), std::filesystem::path(Value(*parsed, "root")), HasFlag(*parsed, "force")}};
    }
    if (command == "list") {
        constexpr std::string_view values[]{"root"};
        auto parsed = ParseArguments(tail, values, {});
        if (!parsed)
            return std::unexpected(parsed.error());
        if (!parsed->positional.empty())
            return std::unexpected(ParseError{"Unexpected argument: " + std::string(parsed->positional.front())});
        return ListCommand{{std::filesystem::path(Value(*parsed, "root"))}};
    }
    if (command == "remove") {
        constexpr std::string_view values[]{"root"};
        constexpr std::string_view flags[]{"keep-data"};
        auto parsed = ParseArguments(tail, values, flags);
        if (!parsed)
            return std::unexpected(parsed.error());
        auto id = OnePositional(*parsed, "id");
        if (!id)
            return std::unexpected(id.error());
        return RemoveCommand{{std::string(*id), std::filesystem::path(Value(*parsed, "root")), HasFlag(*parsed, "keep-data")}};
    }
    if (command == "commands") {
        constexpr std::string_view values[]{"root"};
        constexpr std::string_view flags[]{"json"};
        auto parsed = ParseArguments(tail, values, flags);
        if (!parsed)
            return std::unexpected(parsed.error());
        if (parsed->positional.size() > 1)
            return std::unexpected(ParseError{"Unexpected argument: " + std::string(parsed->positional[1])});
        return CommandsCommand{{parsed->positional.empty() ? std::filesystem::path{} : std::filesystem::path(parsed->positional.front()), std::filesystem::path(Value(*parsed, "root")), HasFlag(*parsed, "json")}};
    }
    if (command == "schema") {
        auto parsed = ParseArguments(tail, {}, {});
        if (!parsed)
            return std::unexpected(parsed.error());
        if (!parsed->positional.empty())
            return std::unexpected(ParseError{"Unexpected argument: " + std::string(parsed->positional.front())});
        return SchemaCommand{};
    }
    return std::unexpected(ParseError{"Unknown command: " + std::string(command)});
}

Status Dispatch(Context& context, const Command& command) {
    return std::visit(
        [&](const auto& typed) -> Status {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, HelpCommand>)
                return Status::Ok;
            else if constexpr (std::is_same_v<T, CreateCommand>)
                return Create(context, typed.options);
            else if constexpr (std::is_same_v<T, BuildCommand>)
                return Build(context, typed.options);
            else if constexpr (std::is_same_v<T, VerifyCommand>)
                return Verify(context, typed.options);
            else if constexpr (std::is_same_v<T, BundleCommand>)
                return Bundle(context, typed.options);
            else if constexpr (std::is_same_v<T, InstallCommand>)
                return Install(context, typed.options);
            else if constexpr (std::is_same_v<T, ListCommand>)
                return List(context, typed.options);
            else if constexpr (std::is_same_v<T, RemoveCommand>)
                return Remove(context, typed.options);
            else if constexpr (std::is_same_v<T, CommandsCommand>)
                return Commands(context, typed.options);
            else if constexpr (std::is_same_v<T, SchemaCommand>)
                return Schema(context);
            else if constexpr (std::is_same_v<T, CleanCommand>)
                return Clean(context, typed.options);
            else {
                const Status built = Build(context, typed.options);
                if (built != Status::Ok)
                    return built;
                if constexpr (std::is_same_v<T, TestCommand>)
                    return Status::Ok;
                return Bundle(context, {.path = typed.options.path, .out_file = {}, .executable = typed.options.executable});
            }
        },
        command
    );
}

int Run(Context& context, std::span<const char* const> args) {
    std::vector<std::string_view> views;
    views.reserve(args.size());
    for (const char* arg : args)
        views.emplace_back(arg);
    std::error_code error;
    auto current = context.filesystem.CurrentPath(error);
    if (error)
        current = ".";
    auto command = ParseCommand(views, current);
    const std::string_view executable = views.empty() ? "wokiext" : views.front();
    if (!command) {
        if (!command.error().message.empty())
            context.diagnostics.Error(command.error().message);
        PrintUsage(context.diagnostics, executable);
        return static_cast<int>(Status::Usage);
    }
    if (std::holds_alternative<HelpCommand>(*command))
        PrintUsage(context.diagnostics, executable);
    try {
        return static_cast<int>(Dispatch(context, *command));
    } catch (const std::exception& exception) {
        context.diagnostics.Error(exception.what());
        return static_cast<int>(Status::Error);
    }
}

int Run(std::span<const char* const> args) {
    Diagnostics diagnostics(std::cout, std::cerr);
    SystemProcessRunner processes;
    SystemFilesystem filesystem;
    Context context{diagnostics, processes, filesystem};
    return Run(context, args);
}

} // namespace wokiext
