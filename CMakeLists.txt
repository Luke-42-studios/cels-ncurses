# cels-ncurses - ncurses-based providers for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window/tui_window.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/tui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tui_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_color.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_draw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_scissor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer/tui_layer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frame/tui_frame.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/tui_widgets.c
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
)

# ============================================================================
# Drawing primitives visual test (standalone, no CELS framework)
# ============================================================================
add_executable(draw_test
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/draw_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_color.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_draw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tui_scissor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer/tui_layer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frame/tui_frame.c
)
target_include_directories(draw_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)
target_compile_definitions(draw_test PRIVATE _XOPEN_SOURCE=700 _DEFAULT_SOURCE)
target_link_libraries(draw_test PRIVATE ${CURSES_LIBRARIES} ${PANEL_LIBRARY} m)
set_target_properties(draw_test PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
