#include "woki/args/args.hpp"

namespace woki {

ArgumentParser::ArgumentParser(std::string program, std::string description)
    : options_(std::move(program), std::move(description)) {}

void ArgumentParser::AddFlag(std::string option, std::string description) {
    options_.add_options()(option, description, cxxopts::value<bool>()->default_value("false")->implicit_value("true"));
}

void ArgumentParser::AddPositional(std::string option) {
    positional_.push_back(std::move(option));
    options_.parse_positional(positional_);
}

void ArgumentParser::SetPositionalHelp(std::string help) {
    positional_help_ = std::move(help);
    options_.positional_help(positional_help_);
}

Result<void> ArgumentParser::Parse(int argc, char** argv) {
    try {
        parse_result_ = options_.parse(argc, argv);
        return Ok();
    } catch (const cxxopts::exceptions::exception& exception) {
        return Err(ErrorCode::ParseInvalidFormat, exception.what());
    }
}

bool ArgumentParser::Parsed() const noexcept {
    return parse_result_.has_value();
}

bool ArgumentParser::Has(std::string_view key) const {
    if (!parse_result_.has_value()) {
        return false;
    }

    return parse_result_->count(std::string(key)) > 0;
}

std::string ArgumentParser::Help() const {
    return options_.help(positional_);
}

} // namespace woki
