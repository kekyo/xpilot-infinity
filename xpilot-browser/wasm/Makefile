# Build SDL3 for the browser driver with the caller's fixed Emscripten SDK.

EMSDK_ROOT ?=
BUILD_DIR ?= $(CURDIR)/../build/wasm
PREFIX ?= $(BUILD_DIR)/prefix
JOBS ?= 4
HOST_CC ?= gcc
NATIVE_PKG_CONFIG_PATH ?= $(PKG_CONFIG_PATH)

SDL_SOURCE := $(abspath $(CURDIR)/../../vendor/sdl3/SDL)
RENDERER_SOURCE := $(abspath $(CURDIR)/../../src/client/sdl)
EMCMAKE := $(EMSDK_ROOT)/upstream/emscripten/emcmake

.PHONY: dependencies check clean

dependencies:
	@command -v flock >/dev/null || { echo "flock is required for browser dependency builds" >&2; exit 1; }
	@mkdir -p "$(BUILD_DIR)"
	flock "$(BUILD_DIR)/dependencies.lock" \
		$(MAKE) --no-print-directory -C "$(CURDIR)" \
		"$(PREFIX)/lib/libSDL3.a" \
		EMSDK_ROOT="$(EMSDK_ROOT)" BUILD_DIR="$(BUILD_DIR)" \
		PREFIX="$(PREFIX)" JOBS="$(JOBS)"

$(PREFIX)/lib/libSDL3.a:
	@test -x "$(EMCMAKE)" || { echo "EMSDK_ROOT must name an installed Emscripten SDK" >&2; exit 1; }
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" "$(EMCMAKE)" cmake \
		-S "$(SDL_SOURCE)" -B "$(BUILD_DIR)/sdl3" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DSDL_SHARED=OFF -DSDL_STATIC=ON \
		-DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
		-DSDL_INSTALL=ON -DSDL_PTHREADS=OFF \
		-DSDL_OPENGLES=ON
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake \
		--build "$(BUILD_DIR)/sdl3" --parallel "$(JOBS)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake \
		--install "$(BUILD_DIR)/sdl3"

check:
	@command -v xvfb-run >/dev/null || { echo "xvfb-run is required" >&2; exit 1; }
	@PKG_CONFIG_PATH="$(NATIVE_PKG_CONFIG_PATH)" pkg-config --exists sdl3 || { echo "native SDL3 pkg-config metadata is required" >&2; exit 1; }
	@mkdir -p "$(BUILD_DIR)"
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror \
		-DSDL_DISABLE_OLD_NAMES \
		-I"$(CURDIR)" -I"$(RENDERER_SOURCE)" \
		$$(PKG_CONFIG_PATH="$(NATIVE_PKG_CONFIG_PATH)" pkg-config --cflags sdl3) \
		"$(CURDIR)/test_driver.c" "$(CURDIR)/driver.c" \
		"$(RENDERER_SOURCE)/renderer.c" \
		"$(RENDERER_SOURCE)/renderer_gl_core.c" \
		-o "$(BUILD_DIR)/native-driver-test" \
		$$(PKG_CONFIG_PATH="$(NATIVE_PKG_CONFIG_PATH)" pkg-config --libs sdl3) \
		-Wl,-rpath,$$(PKG_CONFIG_PATH="$(NATIVE_PKG_CONFIG_PATH)" pkg-config --variable=libdir sdl3) \
		-lGLESv2 -lm
	LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
		xvfb-run -a \
		-s "-screen 0 1280x1024x24 +extension GLX +render -noreset" \
		"$(BUILD_DIR)/native-driver-test"

clean:
	rm -rf "$(BUILD_DIR)/sdl3" \
		"$(PREFIX)/lib/libSDL3.a" \
		"$(PREFIX)/include/SDL3" \
		"$(PREFIX)/lib/cmake/SDL3" \
		"$(PREFIX)/lib/pkgconfig/sdl3.pc"

.PHONY: game-dependencies
game-dependencies: dependencies
	flock "$(BUILD_DIR)/dependencies.lock" \
		$(MAKE) --no-print-directory -C "$(CURDIR)" \
		"$(PREFIX)/lib/libSDL3_ttf.a" "$(PREFIX)/lib/libSDL3_image.a" \
		"$(PREFIX)/lib/libz.a" \
		EMSDK_ROOT="$(EMSDK_ROOT)" BUILD_DIR="$(BUILD_DIR)" \
		PREFIX="$(PREFIX)" JOBS="$(JOBS)"

$(PREFIX)/lib/libSDL3_ttf.a: $(PREFIX)/lib/libSDL3.a
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" "$(EMCMAKE)" cmake \
		-S "$(CURDIR)/../../vendor/sdl3/SDL_ttf" -B "$(BUILD_DIR)/sdl3-ttf" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DSDL3_DIR="$(PREFIX)/lib/cmake/SDL3" -DBUILD_SHARED_LIBS=OFF \
		-DSDLTTF_VENDORED=ON -DSDLTTF_HARFBUZZ=OFF \
		-DSDLTTF_PLUTOSVG=OFF -DSDLTTF_SAMPLES=OFF -DSDLTTF_INSTALL=ON
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --build "$(BUILD_DIR)/sdl3-ttf" --parallel "$(JOBS)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --install "$(BUILD_DIR)/sdl3-ttf"

$(PREFIX)/lib/libSDL3_image.a: $(PREFIX)/lib/libSDL3.a
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" "$(EMCMAKE)" cmake \
		-S "$(CURDIR)/../../vendor/sdl3/SDL_image" -B "$(BUILD_DIR)/sdl3-image" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DSDL3_DIR="$(PREFIX)/lib/cmake/SDL3" -DBUILD_SHARED_LIBS=OFF \
		-DSDLIMAGE_BACKEND_STB=ON -DSDLIMAGE_PNG_LIBPNG=OFF \
		-DSDLIMAGE_AVIF=OFF -DSDLIMAGE_JXL=OFF -DSDLIMAGE_TIF=OFF \
		-DSDLIMAGE_WEBP=OFF -DSDLIMAGE_SAMPLES=OFF -DSDLIMAGE_TESTS=OFF \
		-DSDLIMAGE_INSTALL=ON
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --build "$(BUILD_DIR)/sdl3-image" --parallel "$(JOBS)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --install "$(BUILD_DIR)/sdl3-image"

$(PREFIX)/lib/libz.a:
	mkdir -p "$(BUILD_DIR)/zlib-source"
	cp -a "$(CURDIR)/../../vendor/zlib/." "$(BUILD_DIR)/zlib-source/"
	# zlib 1.2.11's CMake targets both produce libz.a under Emscripten.
	# Its configure script supports a static-only build; its K&R definitions need C99.
	cd "$(BUILD_DIR)/zlib-source" && \
		EM_CONFIG="$(EMSDK_ROOT)/.emscripten" \
		CFLAGS="-O3 -std=c99 -Wno-deprecated-non-prototype" \
		"$(EMSDK_ROOT)/upstream/emscripten/emconfigure" \
		./configure --static --prefix="$(PREFIX)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" $(MAKE) -C "$(BUILD_DIR)/zlib-source" -j"$(JOBS)" libz.a
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" $(MAKE) -C "$(BUILD_DIR)/zlib-source" install
