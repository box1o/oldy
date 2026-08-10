#pragma once

#if defined(__wasm__) && !defined(__clang__)
#error "Woki wasm guests require Clang. Other wasm compilers are not supported."
#endif

#if defined(__clang__) && defined(__wasm__)
#define WOKI_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))
#define WOKI_EXPORT(name) __attribute__((export_name(name)))
#else
#define WOKI_IMPORT(module, name)
#define WOKI_EXPORT(name)
#endif

#if defined(__clang__) || defined(__GNUC__)
#define WOKI_WEAK __attribute__((weak))
#else
#define WOKI_WEAK
#endif
