-include scripts/emdawnwebgpu.env

BUILD_DIR      := build
WEB_BUILD_DIR  := build-web
BUILD_TYPE     := Release
WEB_PORT       ?= 8080
WEB_TARGET     := studio
NUM_JOBS       ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
CONFIG         ?= config/conf/studio.yaml
PREFIX         ?= /usr/local
TMP_ROOT       ?= $(CURDIR)/.cache/tmp
CCACHE_DIR     ?= $(CURDIR)/.cache/ccache
EM_CACHE       ?= $(CURDIR)/.cache/emscripten
GFX_GENERATED_DIR ?= $(CURDIR)/$(BUILD_DIR)/modules/gfx/generated
WOKI_EXTENSION_WITH_WASMTIME ?= ON
WASMTIME_PROVIDER ?= prebuilt
WOKI_GFX_FETCH_TINT ?= ON
WOKI_GFX_REQUIRE_TINT ?= ON
GFX_COOKED_DIR ?= $(CURDIR)/$(BUILD_DIR)/modules/gfx/assets/cooked

# Use Clang with libc++ to match the Dawn dependency
CXX            := $(shell command -v clang++ 2>/dev/null || command -v c++ 2>/dev/null || echo c++)
ifeq ($(findstring clang,$(CXX)),clang)
	CXXFLAGS       := -stdlib=libc++
	LDFLAGS        := -stdlib=libc++
endif

.DEFAULT_GOAL := run
.PHONY: configure build rebuild clean install run test all format format-check \
	web-prerequisites configure-web build-web web

RUN_ARGS       := $(if $(CONFIG),--config $(CONFIG),)

configure:
	@echo "→ Configuring native ($(BUILD_TYPE))..."
	@echo "  Wasmtime: $(WOKI_EXTENSION_WITH_WASMTIME) (provider=$(WASMTIME_PROVIDER))"
	@mkdir -p $(BUILD_DIR) $(TMP_ROOT) $(CCACHE_DIR)
	@cd $(BUILD_DIR) && CCACHE_DIR=$(CCACHE_DIR) TMPDIR=$(TMP_ROOT) TEMP=$(TMP_ROOT) TMP=$(TMP_ROOT) cmake .. \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_CXX_FLAGS="$(CXXFLAGS)" \
		-DWOKI_EXTENSION_WITH_WASMTIME=$(WOKI_EXTENSION_WITH_WASMTIME) \
		-DWOKI_WASMTIME_PROVIDER=$(WASMTIME_PROVIDER) \
		-DWOKI_GFX_FETCH_TINT=$(WOKI_GFX_FETCH_TINT) \
		-DWOKI_GFX_REQUIRE_TINT=$(WOKI_GFX_REQUIRE_TINT)

build: configure
	@echo "→ Building native..."
	@CCACHE_DIR=$(CCACHE_DIR) TMPDIR=$(TMP_ROOT) TEMP=$(TMP_ROOT) TMP=$(TMP_ROOT) cmake --build $(BUILD_DIR) -j$(NUM_JOBS)
	@echo "✓ Native build complete"

run: build
	@echo "→ Running studio (native)..."
	@cd $(BUILD_DIR)/bin && ./studio $(RUN_ARGS)

test: build
	@echo "→ Running tests..."
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

install:
	@echo "→ Installing native to $(PREFIX)..."
	@cmake --install $(BUILD_DIR) --prefix $(PREFIX)
	@echo "✓ Install complete"

rebuild: clean build

debug:
	@$(MAKE) run BUILD_TYPE=Debug

release:
	@$(MAKE) run BUILD_TYPE=Release

all: build

format:
	@python3 scripts/format.py

format-check:
	@python3 scripts/format.py --check

web-prerequisites: configure
	@echo "→ Preparing host-generated web assets..."
	@CCACHE_DIR=$(CCACHE_DIR) TMPDIR=$(TMP_ROOT) TEMP=$(TMP_ROOT) TMP=$(TMP_ROOT) cmake --build $(BUILD_DIR) --target woki_gfx_generated_abi woki_gfx_cooked_assets -j$(NUM_JOBS)

configure-web: web-prerequisites
	@echo "→ Configuring web ($(BUILD_TYPE))..."
	@mkdir -p $(WEB_BUILD_DIR) $(TMP_ROOT) $(CCACHE_DIR) $(EM_CACHE)
	@cd $(WEB_BUILD_DIR) && CCACHE_DIR=$(CCACHE_DIR) EM_CACHE=$(EM_CACHE) TMPDIR=$(TMP_ROOT) TEMP=$(TMP_ROOT) TMP=$(TMP_ROOT) emcmake cmake .. \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DWOKI_GFX_PREGENERATED_ABI_DIR=$(GFX_GENERATED_DIR) \
		-DWOKI_GFX_PRECOOKED_ASSET_DIR=$(GFX_COOKED_DIR) \
		-DEMDAWNWEBGPU_PORT="$(EMDAWNWEBGPU_PORT)"



build-web: configure-web
	@echo "→ Building web..."
	@CCACHE_DIR=$(CCACHE_DIR) EM_CACHE=$(EM_CACHE) TMPDIR=$(TMP_ROOT) TEMP=$(TMP_ROOT) TMP=$(TMP_ROOT) cmake --build $(WEB_BUILD_DIR) --target $(WEB_TARGET) -j$(NUM_JOBS)
	@cp web/favicon.svg $(WEB_BUILD_DIR)/favicon.svg 2>/dev/null || true
	@cp config/icons/favicon.ico $(WEB_BUILD_DIR)/favicon.ico 2>/dev/null || true
	@cp config/icons/apple-touch-icon.png $(WEB_BUILD_DIR)/apple-touch-icon.png 2>/dev/null || true
	@cp config/icons/woki-32.png $(WEB_BUILD_DIR)/woki-32.png 2>/dev/null || true
	@cp config/icons/woki-64.png $(WEB_BUILD_DIR)/woki-64.png 2>/dev/null || true
	@cp web/woki-sad.svg $(WEB_BUILD_DIR)/woki-sad.svg 2>/dev/null || true
	@cp $(WEB_BUILD_DIR)/$(WEB_TARGET).html $(WEB_BUILD_DIR)/index.html
	@echo "✓ Web build complete"

web: build-web
	@echo "→ Running web (emrun)..."
	@emrun --no_browser --hostname localhost --port $(WEB_PORT) $(WEB_BUILD_DIR)
	@echo "Open: http://localhost:$(WEB_PORT)/"

clean:
	@echo "→ Cleaning build output..."
	@rm -rf build build-* $(WEB_BUILD_DIR) ext/build extensions/*/build Testing/ .bin/ wokiext wokiext.exe woki-studio woki-studio.exe
	@echo "✓ Clean complete"
