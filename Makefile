CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
PKG_CONFIG ?= pkg-config

SFML_CFLAGS := $(shell $(PKG_CONFIG) --cflags sfml-graphics sfml-window sfml-system)
SFML_LIBS := $(shell $(PKG_CONFIG) --libs sfml-graphics sfml-window sfml-system)

SOURCES := src/main.cpp src/game.cpp src/ai.cpp src/renderer.cpp src/assets.cpp src/selftest.cpp
HEADERS := src/game.hpp src/ai.hpp src/renderer.hpp src/assets.hpp src/selftest.hpp

bomberboy: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SFML_CFLAGS) $(SOURCES) -o $@ $(SFML_LIBS)

# Rebuilds the pixel-art sprites in assets/sprites from tools/generate_assets.cpp.
assets:
	$(CXX) $(CXXFLAGS) $(SFML_CFLAGS) tools/generate_assets.cpp -o generate_assets $(SFML_LIBS)
	./generate_assets assets/sprites

test: bomberboy
	./bomberboy --selftest

run: bomberboy
	./bomberboy

clean:
	rm -f bomberboy generate_assets

.PHONY: test run clean assets
