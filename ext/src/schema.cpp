#include "cli_internal.hpp"
#include "manifest_schema.hpp"

namespace wokiext {

Status Schema(Context& context) {
    context.diagnostics.Out() << kManifestSchema;
    return Status::Ok;
}

} // namespace wokiext
