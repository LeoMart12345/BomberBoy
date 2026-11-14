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
  src/selftest.cpp
)

target_include_directories(bomberboy PRIVATE src)
target_link_libraries(bomberboy PRIVATE sfml-graphics sfml-window sfml-system)

if(MSVC)
  target_compile_options(bomberboy PRIVATE /W4)
else()
  target_compile_options(bomberboy PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME bomberboy_selftest COMMAND bomberboy --selftest)
