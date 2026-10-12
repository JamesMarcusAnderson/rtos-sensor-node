cmake_minimum_required(VERSION 3.13)

# Requires both trees on the path, e.g.:
#   cmake -B build \
#     -DPICO_SDK_PATH=$HOME/pico-sdk \
#     -DFREERTOS_KERNEL_PATH=$HOME/FreeRTOS-Kernel \
#     -DPICO_BOARD=pico
#   cmake --build build -j
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(rtos_sensor_node C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# FreeRTOSConfig.h must be reachable as an include *directory*, not merely as a
# sibling of main.c: the kernel port's freertos_sdk_config.h pulls it in via
# pico/config.h, which is injected into every target including the SDK's own.
include_directories(${CMAKE_CURRENT_LIST_DIR})

set(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}" CACHE PATH "FreeRTOS kernel source tree")
if(NOT EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/CMakeLists.txt")
    message(FATAL_ERROR
        "FreeRTOS kernel not found at ${FREERTOS_KERNEL_PATH}. "
        "Pass -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel (git submodule recommended).")
endif()
add_subdirectory(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040 FreeRTOS-Kernel)

add_executable(sensor_node main.c)

target_link_libraries(sensor_node
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_watchdog
    FreeRTOS-Kernel-Heap4
)

# monitor.py speaks to the USB CDC serial port, so stdio goes over USB.
pico_enable_stdio_usb(sensor_node 1)
pico_enable_stdio_uart(sensor_node 0)

pico_add_extra_outputs(sensor_node)
