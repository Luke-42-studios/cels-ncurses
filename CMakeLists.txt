# cels-ncurses - ncurses-based providers for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)

# ============================================================================
# Find ncurses (wide-character variant)
# ============================================================================
set(CURSES_NEED_WIDE TRUE)
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)

# ============================================================================
# INTERFACE library target
# ============================================================================
add_library(cels-ncurses INTERFACE)

target_sources(cels-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window/tui_window.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/tui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/tui_renderer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tui_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_color.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_draw.c
)

target_include_directories(cels-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_compile_definitions(cels-ncurses INTERFACE
    _XOPEN_SOURCE=700
    _DEFAULT_SOURCE
)

target_link_libraries(cels-ncurses INTERFACE
    ${CURSES_LIBRARIES}
    cels
    m
)
