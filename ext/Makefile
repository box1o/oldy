BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= /usr/local
NUM_JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

.DEFAULT_GOAL := build
.PHONY: configure build install clean test

configure:
	@cmake -B $(BUILD_DIR) -S .. \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	@cmake --build $(BUILD_DIR) --target wokiext -j$(NUM_JOBS)

install: build
	@cmake --install $(BUILD_DIR) --prefix $(PREFIX)

test: build
	@cmake --build $(BUILD_DIR) --target woki_extensions -j$(NUM_JOBS)
	@$(BUILD_DIR)/ext/wokiext verify ../extensions/kitty

clean:
	@rm -rf $(BUILD_DIR)
