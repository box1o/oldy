#pragma once

#include <optional>

#include <woki/events/base.hpp>
#include <woki/ext/application_event.hpp>

namespace woki {

[[nodiscard]] std::optional<ext::EncodedApplicationEvent> EncodeExtensionEvent(const events::Event& event) noexcept;

} // namespace woki
