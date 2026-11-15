CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Isrc
PKG_CONFIG ?= pkg-config

# pkg-config finds SFML 3 automatically when it is installed system-wide. For a
# hand-installed prefix, pass SFML_PREFIX=/path/to/sfml3 (the same value CMake
# accepts) or export it in the environment. Without one, reuse the SFML install
# that CMake already found for this tree, so `make` and `cmake --build` agree.
ifneq ($(strip $(SFML_PREFIX)),)
  BB_SFML_PREFIX := $(strip $(SFML_PREFIX))
else
  SFML_CMAKE_DIR := $(shell sed -n 's/^SFML_DIR:PATH=//p' build/CMakeCache.txt 2>/dev/null)
  ifneq ($(strip $(SFML_CMAKE_DIR)),)
    BB_SFML_PREFIX := $(abspath $(SFML_CMAKE_DIR)/../../..)
  endif
endif
ifneq ($(BB_SFML_PREFIX),)
  PKG_CONFIG_PATH := $(BB_SFML_PREFIX)/lib/pkgconfig:$(BB_SFML_PREFIX)/lib64/pkgconfig$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))
  export PKG_CONFIG_PATH
endif

SFML_PACKAGES := sfml-graphics sfml-window sfml-system
SFML_VERSION := $(shell $(PKG_CONFIG) --modversion sfml-graphics 2>/dev/null)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
  ifneq ($(firstword $(subst ., ,$(SFML_VERSION))),3)
    $(error BomberBoy needs SFML 3 (found '$(SFML_VERSION)'); install it, pass SFML_PREFIX=/path/to/sfml3, or use CMake)
  endif
endif

SFML_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(SFML_PACKAGES))
SFML_LIBS := $(shell $(PKG_CONFIG) --libs $(SFML_PACKAGES))
SFML_LIBDIR := $(shell $(PKG_CONFIG) --variable=libdir sfml-graphics)
comma := ,
SFML_RPATH := $(if $(SFML_LIBDIR),-Wl$(comma)-rpath$(comma)$(SFML_LIBDIR))

SOURCES := src/main.cpp src/core/game.cpp src/core/ai.cpp src/render/renderer.cpp \
           src/render/assets.cpp src/cli/cli.cpp src/tests/selftest.cpp
HEADERS := src/core/game.hpp src/core/ai.hpp src/render/renderer.hpp \
           src/render/assets.hpp src/cli/cli.hpp src/tests/selftest.hpp

bomberboy: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SFML_CFLAGS) $(SOURCES) -o $@ $(SFML_LIBS) $(SFML_RPATH)

# Rebuilds the pixel-art sprites in assets/sprites from tools/generate_assets.cpp.
assets:
	$(CXX) $(CXXFLAGS) $(SFML_CFLAGS) tools/generate_assets.cpp -o generate_assets $(SFML_LIBS) $(SFML_RPATH)
	./generate_assets assets/sprites

test: bomberboy
	./bomberboy --selftest

run: bomberboy
	./bomberboy

clean:
	rm -f bomberboy generate_assets

.PHONY: test run clean assets
