#pragma once

#if defined(__clang__) && defined(__wasm__)
#define WOKI_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))
#define WOKI_EXPORT(name) __attribute__((export_name(name)))
#else
#define WOKI_IMPORT(module, name)
#define WOKI_EXPORT(name)
#endif
