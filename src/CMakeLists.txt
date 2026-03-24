# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

#SET(GCC_COVERAGE_COMPILE_FLAGS "-L")

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(main C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
add_executable(main
i2c_flexigpio.cpp
i2c_slave.c
main.cpp)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_add_extra_outputs(main)

# Pull in pico libraries that we need
target_link_libraries(main hardware_i2c pico_stdlib hardware_pio pico_malloc pico_mem_ops)
#target_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

