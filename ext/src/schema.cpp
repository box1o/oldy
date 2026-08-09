#include <iostream>

#include "wokiext/cli.hpp"
#include "manifest_schema.hpp"

namespace wokiext {

Status Schema() {
    std::cout << kManifestSchema;
    return Status::Ok;
}

} // namespace wokiext
