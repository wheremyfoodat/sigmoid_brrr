cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

project(sigmoid)

option(USE_SYSTEM_SDL2 "Use the system's SDL2 package" OFF)

message(STATUS "Verilator root: ${VERILATOR_ROOT}")
find_package(verilator REQUIRED HINTS ${VERILATOR_ROOT} $ENV{VERILATOR_ROOT} /opt/homebrew/opt/verilator)
if (NOT verilator_FOUND)
    message(FATAL_ERROR "Verilator was not found. Please install it and set the VERILATOR_ROOT environment variable")
endif()

set(TESTBENCH_SOURCE src/testbench.cpp src/ui.cpp)
set(THIRD_PARTY_SOURCE_FILES
    third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_demo.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
)

add_executable(sigmoid ${TESTBENCH_SOURCE} ${THIRD_PARTY_SOURCE_FILES})

target_include_directories(sigmoid PRIVATE include)
target_include_directories(sigmoid PRIVATE ${FMT_INCLUDE_DIR})
target_include_directories(sigmoid PRIVATE third_party third_party/imgui third_party/imgui/backends)

target_link_libraries(sigmoid PRIVATE fmt::fmt glad)

if (USE_SYSTEM_SDL2)
    find_package(SDL2 CONFIG REQUIRED)
    target_link_libraries(sigmoid PUBLIC SDL2::SDL2)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/SDL2)
    target_link_libraries(sigmoid PUBLIC SDL2-static)
endif()

add_subdirectory(third_party/glad)
add_subdirectory(third_party/fmt)

# Compilation order generated automatically by Vivado
set(RTL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../rtl)
set(RTL_SOURCE
    ${RTL_DIR}/bf16_cmp.sv ${RTL_DIR}/bf16_constants.sv ${RTL_DIR}/bf16/lampFPU_pkg.sv
    ${RTL_DIR}/bf16_units.sv ${RTL_DIR}/bf16/lampFPU_addsub_comb.sv ${RTL_DIR}/bf16/lampFPU_cmp_comb.sv
    ${RTL_DIR}/bf16/lampFPU_div_comb.sv ${RTL_DIR}/bf16/lampFPU_f2i_comb.sv ${RTL_DIR}/bf16/lampFPU_fractDiv_comb.sv
    ${RTL_DIR}/bf16/lampFPU_i2f_comb.sv ${RTL_DIR}/bf16/lampFPU_mul_comb.sv ${RTL_DIR}/single_cycle_fpu.sv
    ${RTL_DIR}/polynomial_2nd_degree.sv ${RTL_DIR}/sigmoid.sv ${RTL_DIR}/sigmoid_pipelined.sv
)

verilate(
    sigmoid
    PREFIX sigmoid_t
    SOURCES ${RTL_SOURCE}
    TOP_MODULE sigmoid_pipelined
    VERILATOR_ARGS -Wall -Wno-fatal
)