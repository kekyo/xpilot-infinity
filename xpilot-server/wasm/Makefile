# Emscripten is prepared by emsdk-env; never discover a system SDK.
EMSDK_ROOT ?=
BUILD_DIR ?= $(CURDIR)/../build/wasm
PREFIX ?= $(BUILD_DIR)/prefix
JOBS ?= 4
EMCMAKE := $(EMSDK_ROOT)/upstream/emscripten/emcmake

.PHONY: dependencies
dependencies: $(PREFIX)/lib/libexpat.a $(PREFIX)/lib/libz.a

$(PREFIX)/lib/libexpat.a:
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" "$(EMCMAKE)" cmake \
		-S "$(CURDIR)/../../vendor/expat/expat" -B "$(BUILD_DIR)/expat" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DEXPAT_SHARED_LIBS=OFF -DEXPAT_BUILD_TOOLS=OFF \
		-DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TESTS=OFF \
		-DEXPAT_BUILD_DOCS=OFF -DEXPAT_BUILD_PKGCONFIG=OFF
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --build "$(BUILD_DIR)/expat" --parallel "$(JOBS)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --install "$(BUILD_DIR)/expat"

$(PREFIX)/lib/libz.a:
	mkdir -p "$(BUILD_DIR)/zlib-source"
	cp -a "$(CURDIR)/../../vendor/zlib/." "$(BUILD_DIR)/zlib-source/"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" "$(EMCMAKE)" cmake \
		-S "$(BUILD_DIR)/zlib-source" -B "$(BUILD_DIR)/zlib" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_TESTING=OFF
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --build "$(BUILD_DIR)/zlib" --parallel "$(JOBS)"
	EM_CONFIG="$(EMSDK_ROOT)/.emscripten" cmake --install "$(BUILD_DIR)/zlib"
