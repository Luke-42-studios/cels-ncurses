# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# cels-ncurses - ncurses-based providers for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)
project(cels-ncurses VERSION 0.2.0 LANGUAGES C)

# ============================================================================
# Dependencies
# ============================================================================
# cels-ncurses is an addon library -- the consumer must provide cels.
# Typical consumer CMakeLists.txt:
#
#   FetchContent_Declare(cels
#       GIT_REPOSITORY https://github.com/42-Galaxies/cels.git
#       GIT_TAG        v0.5.2)
#   FetchContent_Declare(cels-ncurses
#       GIT_REPOSITORY https://github.com/42-Galaxies/cels-ncurses.git
#       GIT_TAG        v0.2.0)
#   FetchContent_MakeAvailable(cels cels-ncurses)
#
#   target_link_libraries(your_app PRIVATE cels cels-ncurses)
#
if(NOT TARGET cels)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # Top-level build (development) -- use sibling cels directory
        # Point FetchContent at cels's already-downloaded deps to avoid network clones
        set(FETCHCONTENT_SOURCE_DIR_FLECS
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/flecs-src" CACHE PATH "")
        set(FETCHCONTENT_SOURCE_DIR_YYJSON
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/yyjson-src" CACHE PATH "")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cels cels)
    else()
        message(FATAL_ERROR
            "cels-ncurses requires the 'cels' target. "
            "Add cels via FetchContent or add_subdirectory before cels-ncurses.")
    endif()
endif()

# ============================================================================
# Find ncurses (wide-character variant)
# ============================================================================
set(CURSES_NEED_WIDE TRUE)
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)
find_library(PANEL_LIBRARY NAMES panelw panel)

# ============================================================================
# INTERFACE library target
# ============================================================================
add_library(cels-ncurses INTERFACE)

target_sources(cels-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ncurses_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window/tui_window.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window/tui_spawn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/tui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_color.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_draw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_scissor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_subcell.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer/tui_surface_panel.c
)

target_include_directories(cels-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_compile_definitions(cels-ncurses INTERFACE
    _XOPEN_SOURCE=700
    _DEFAULT_SOURCE
    CELS_HAS_ECS
)

target_link_libraries(cels-ncurses INTERFACE
    ${CURSES_LIBRARIES}
    ${PANEL_LIBRARY}
    cels
    m
    $<$<PLATFORM_ID:Linux>:util>
)

# ============================================================================
# Examples (only when building this repo directly)
# ============================================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Minimal example (uses CELS framework via cels-ncurses)
    add_executable(minimal examples/minimal.c)
    target_link_libraries(minimal PRIVATE cels-ncurses)
    set_target_properties(minimal PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
endif()

