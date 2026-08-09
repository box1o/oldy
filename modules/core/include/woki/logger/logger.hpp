#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <string_view>

#ifdef __EMSCRIPTEN__
#include <format>
#else
#include <fmt/core.h>
#include <spdlog/logger.h>
#endif

namespace slog {

enum class Level { Trace, Debug, Info, Warn, Error, Critical, Off };

void Configure(std::string name = "woki",
    Level level = Level::Info,
    const std::string& pattern = "[%H:%M:%S] [%^%l%$] %v",
    const std::string& logFile = "",
    std::size_t maxFileSize = 5 * 1024 * 1024,
    std::size_t maxFiles = 3);

namespace detail {

#ifndef __EMSCRIPTEN__
using LoggerHandle = spdlog::logger;
std::shared_ptr<LoggerHandle> Logger() noexcept;
#else
void LogWebTrace(const char* msg);
void LogWebDebug(const char* msg);
void LogWebInfo(const char* msg);
void LogWebWarn(const char* msg);
void LogWebError(const char* msg);
void LogWebCritical(const char* msg);
#endif

} // namespace detail

template <typename... Args>
inline void Trace(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->trace(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebTrace(msg.c_str());
#endif
}

template <typename... Args>
inline void Debug(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->debug(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebDebug(msg.c_str());
#endif
}

template <typename... Args>
inline void Info(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->info(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebInfo(msg.c_str());
#endif
}

template <typename... Args>
inline void Warn(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->warn(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebWarn(msg.c_str());
#endif
}

template <typename... Args>
inline void Error(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->error(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebError(msg.c_str());
#endif
}

template <typename... Args>
inline void Critical(std::string_view fmt, Args&&... args) {
#ifndef __EMSCRIPTEN__
    if (auto l = detail::Logger())
        l->critical(fmt::runtime(fmt), std::forward<Args>(args)...);
#else
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    detail::LogWebCritical(msg.c_str());
#endif
}

} // namespace slog
