#include "woki/logger/logger.hpp"

#ifdef __EMSCRIPTEN__

#include <emscripten/em_js.h>

EM_JS(void, js_log_trace, (const char* msg), { console.debug(UTF8ToString(msg)); });
EM_JS(void, js_log_debug, (const char* msg), { console.info(UTF8ToString(msg)); });
EM_JS(void, js_log_info, (const char* msg), { console.info(UTF8ToString(msg)); });
EM_JS(void, js_log_warn, (const char* msg), { console.warn(UTF8ToString(msg)); });
EM_JS(void, js_log_error, (const char* msg), { console.error(UTF8ToString(msg)); });
EM_JS(void, js_log_critical, (const char* msg), { console.error(UTF8ToString(msg)); });

namespace slog {

void Configure(std::string, Level, const std::string&, const std::string&, std::size_t, std::size_t) {}

namespace detail {

void LogWebTrace(const char* msg) {
    js_log_trace(msg);
}

void LogWebDebug(const char* msg) {
    js_log_debug(msg);
}

void LogWebInfo(const char* msg) {
    js_log_info(msg);
}

void LogWebWarn(const char* msg) {
    js_log_warn(msg);
}

void LogWebError(const char* msg) {
    js_log_error(msg);
}

void LogWebCritical(const char* msg) {
    js_log_critical(msg);
}

} // namespace detail

} // namespace slog

#else

#include <atomic>
#include <memory>
#include <vector>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace slog {

namespace detail {

std::shared_ptr<spdlog::logger>& LoggerRef() {
    static std::shared_ptr<spdlog::logger> logger;
    return logger;
}

std::shared_ptr<spdlog::logger> Logger() noexcept {
    return std::atomic_load_explicit(&LoggerRef(), std::memory_order_acquire);
}

} // namespace detail

void Configure(std::string name, Level level, const std::string& pattern, const std::string& logFile, std::size_t maxFileSize, std::size_t maxFiles) {
    auto sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>{};

    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (!logFile.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, maxFileSize, maxFiles));
    }

    auto logger = std::make_shared<spdlog::logger>(std::move(name), sinks.begin(), sinks.end());

    logger->set_level([&] {
        using L = Level;
        using SL = spdlog::level::level_enum;
        switch (level) {
            case L::Trace:
                return SL::trace;
            case L::Debug:
                return SL::debug;
            case L::Info:
                return SL::info;
            case L::Warn:
                return SL::warn;
            case L::Error:
                return SL::err;
            case L::Critical:
                return SL::critical;
            case L::Off:
                return SL::off;
        }
        return SL::info;
    }());

    logger->set_pattern(pattern);
    logger->flush_on(spdlog::level::warn);

    std::atomic_store_explicit(&detail::LoggerRef(), std::move(logger), std::memory_order_release);
}

} // namespace slog

#endif
