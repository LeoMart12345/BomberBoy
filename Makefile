CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
PKG_CONFIG ?= pkg-config

SFML_CFLAGS := $(shell $(PKG_CONFIG) --cflags sfml-graphics sfml-window sfml-system)
SFML_LIBS := $(shell $(PKG_CONFIG) --libs sfml-graphics sfml-window sfml-system)

SOURCES := src/main.cpp src/game.cpp src/renderer.cpp src/selftest.cpp
HEADERS := src/game.hpp src/renderer.hpp src/selftest.hpp

bomberboy: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SFML_CFLAGS) $(SOURCES) -o $@ $(SFML_LIBS)

test: bomberboy
	./bomberboy --selftest

run: bomberboy
	./bomberboy

clean:
	rm -f bomberboy

.PHONY: test run clean
