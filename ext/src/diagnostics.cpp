#include <ostream>

#include "cli_internal.hpp"

namespace wokiext {

Diagnostics::Diagnostics(std::ostream& output, std::ostream& error)
    : output_(&output),
      error_(&error) {}

std::ostream& Diagnostics::Out() const {
    return *output_;
}

std::ostream& Diagnostics::Err() const {
    return *error_;
}

void Diagnostics::Info(std::string_view message) const {
    *output_ << message << '\n';
}

void Diagnostics::Error(std::string_view message) const {
    *error_ << message << '\n';
}

void Diagnostics::Warning(std::string_view message) const {
    *error_ << "Warning: " << message << '\n';
}

} // namespace wokiext
