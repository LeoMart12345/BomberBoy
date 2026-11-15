cmake_minimum_required(VERSION 3.24)
project(BomberBoy VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SFML 3 is required. When it is installed, use the package directly, which is
# the normal SFML 3 CMake workflow. Optional fallback: fetch and build SFML 3.0.2
# so a plain `cmake -S . -B build` also works on machines that only have SFML 2.
# The fallback still needs SFML's own development dependencies (X11/OpenGL,
# FreeType, and their development packages); see README.md.
option(BOMBERBOY_FETCH_SFML "Fetch and build SFML 3 when it is not installed" ON)

# SFML 3 can also live in a prefix that is neither on CMAKE_PREFIX_PATH nor in a
# default install location (a hand-built SFML, a local toolchain directory, ...).
# Accept it through SFML_PREFIX - as a cache variable or an environment
# variable - and, as one more try, through the prefix that pkg-config reports.
set(SFML_PREFIX "" CACHE PATH "Prefix of an SFML 3 installation")

find_package(SFML 3 COMPONENTS Graphics Window System QUIET)

if(NOT SFML_FOUND)
  set(BB_SFML_PREFIX "")
  if(SFML_PREFIX)
    set(BB_SFML_PREFIX "${SFML_PREFIX}")
  elseif(DEFINED ENV{SFML_PREFIX} AND NOT "$ENV{SFML_PREFIX}" STREQUAL "")
    set(BB_SFML_PREFIX "$ENV{SFML_PREFIX}")
  else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(BB_SFML_PKGCONFIG QUIET sfml-graphics>=3)
      if(BB_SFML_PKGCONFIG_FOUND)
        set(BB_SFML_PREFIX "${BB_SFML_PKGCONFIG_PREFIX}")
      endif()
    endif()
  endif()

  if(BB_SFML_PREFIX)
    message(STATUS "Looking for SFML 3 in ${BB_SFML_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${BB_SFML_PREFIX}")
    find_package(SFML 3 COMPONENTS Graphics Window System QUIET)
  endif()
endif()

if(NOT SFML_FOUND)
  if(NOT BOMBERBOY_FETCH_SFML)
    message(FATAL_ERROR
      "SFML 3 was not found. Install SFML 3 and set CMAKE_PREFIX_PATH, "
      "SFML_DIR or SFML_PREFIX, or configure with -DBOMBERBOY_FETCH_SFML=ON "
      "to build it from source.")
  endif()
  message(STATUS "SFML 3 not found - fetching and building SFML 3.0.2 from source")
  # The source build needs X11's development files. Check them up front so a
  # missing package reads as one clear sentence instead of an error from deep
  # inside SFML's own CMake code.
  if(UNIX AND NOT APPLE)
    find_package(X11 QUIET COMPONENTS Xrandr Xcursor Xi)
    if(NOT X11_FOUND)
      message(FATAL_ERROR
        "Building SFML 3 from source needs the X11 development packages "
        "(Xrandr, Xcursor and Xi; e.g. libxrandr-dev libxcursor-dev libxi-dev "
        "on Debian/Ubuntu). Install them, or install SFML 3 and point "
        "SFML_PREFIX/CMAKE_PREFIX_PATH at it.")
    endif()
  endif()
  include(FetchContent)
  set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SFML
    URL https://github.com/SFML/SFML/archive/refs/tags/3.0.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(SFML)
endif()

add_executable(bomberboy
  src/main.cpp
  src/core/game.cpp
  src/core/ai.cpp
  src/render/renderer.cpp
  src/render/assets.cpp
  src/cli/cli.cpp
  src/tests/selftest.cpp
)

target_include_directories(bomberboy PRIVATE src)
target_link_libraries(bomberboy PRIVATE SFML::Graphics)

# Regenerates the pixel-art sprites in assets/sprites (run manually when editing
# tools/generate_assets.cpp).
add_executable(generate_assets EXCLUDE_FROM_ALL tools/generate_assets.cpp)
target_link_libraries(generate_assets PRIVATE SFML::Graphics)

# Keep the art next to the binary so `./build/bomberboy` finds it too.
add_custom_command(TARGET bomberboy POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)

if(MSVC)
  target_compile_options(bomberboy PRIVATE /W4)
else()
  target_compile_options(bomberboy PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME bomberboy_selftest COMMAND bomberboy --selftest)
