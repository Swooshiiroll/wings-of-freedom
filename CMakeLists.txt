cmake_minimum_required(VERSION 3.28)

# The version line is read and written by tools/dev.sh (stack_get_version / stack_set_version):
# keep "project(wings_of_freedom VERSION x.y.z" on one line.
project(wings_of_freedom VERSION 0.1.0
  DESCRIPTION "A pixel-art isometric air-combat game"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No C++20 modules here: skip CMake's per-file module scan (faster builds, and ccache can cache
# the compiles; it can't with the scan on).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# Static MSVC runtime: the exe runs without the player installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WOF_BUILD_GAME "Build the SDL3 game executable (tests only need the core library)" ON)
option(WOF_WERROR "Treat compiler warnings in this project's code as errors" OFF)

include(CTest)
include(FetchContent)

# Warning flags for this project's own targets (never applied to SDL or doctest).
add_library(wof_warnings INTERFACE)
if(MSVC)
  target_compile_options(wof_warnings INTERFACE /W4 /permissive- /utf-8
    $<$<BOOL:${WOF_WERROR}>:/WX>)
else()
  target_compile_options(wof_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    $<$<BOOL:${WOF_WERROR}>:-Werror>)
endif()

# --- Dependencies: pinned release archives, verified by SHA256 -----------------------------------
if(WOF_BUILD_GAME)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(SDL3)
endif()

if(BUILD_TESTING)
  set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
  set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/releases/download/v2.5.3/doctest-v2.5.3.tar.gz
    URL_HASH SHA256=eb4a39729d091e43d4d593888bf4d145ee9a0f0e13f7404484b6f2fb60d5d50f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(doctest)
endif()

# --- Project -------------------------------------------------------------------------------------
add_subdirectory(src/core)
if(WOF_BUILD_GAME)
  add_subdirectory(src/game)
endif()
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
