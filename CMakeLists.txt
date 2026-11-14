cmake_minimum_required(VERSION 3.16)
project(BomberBoy VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(bomberboy
  src/main.cpp
  src/game.cpp
  src/renderer.cpp
  src/assets.cpp
  src/selftest.cpp
)

target_include_directories(bomberboy PRIVATE src)
target_link_libraries(bomberboy PRIVATE sfml-graphics sfml-window sfml-system)

# Regenerates the pixel-art sprites in assets/sprites (run manually when editing
# tools/generate_assets.cpp).
add_executable(generate_assets EXCLUDE_FROM_ALL tools/generate_assets.cpp)
target_link_libraries(generate_assets PRIVATE sfml-graphics sfml-window sfml-system)

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
