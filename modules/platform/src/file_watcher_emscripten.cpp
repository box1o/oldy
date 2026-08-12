#include "file_watcher_internal.hpp"

namespace woki::detail {

Result<scope<FileWatcherBackend>> CreateFileWatcherBackend(const std::filesystem::path&, FileWatcherState&) {
    return Err(ErrorCode::InvalidState, "File watching is unsupported on Emscripten");
}

} // namespace woki::detail
