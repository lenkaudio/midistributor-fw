# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
   set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(PICO_SDK_PATH ${USERHOME}/.pico-sdk/sdk/2.0.0)
set(PICO_TOOLCHAIN_PATH ${USERHOME}/.pico-sdk/toolchain/13_2_Rel1)
set(pioasm_HINT ${USERHOME}/.pico-sdk/tools/2.0.0/pioasm)
if(EXISTS ${pioasm_HINT})
    set(pioasm_DIR ${pioasm_HINT})
endif()
set(picotool_HINT ${USERHOME}/.pico-sdk/picotool/2.0.0/picotool)
if(EXISTS ${picotool_HINT})
    set(picotool_DIR ${picotool_HINT})
endif()
if(PICO_TOOLCHAIN_PATH MATCHES "RISCV")
    set(PICO_PLATFORM rp2350-riscv CACHE STRING "Pico Platform")
    if(PICO_TOOLCHAIN_PATH MATCHES "COREV")
        set(PICO_COMPILER pico_riscv_gcc_zcb_zcmp)
    endif()
endif()
# ====================================================================================
# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
   set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(PICO_SDK_PATH ${USERHOME}/.pico-sdk/sdk/2.0.0)
set(PICO_TOOLCHAIN_PATH ${USERHOME}/.pico-sdk/toolchain/13_2_Rel1)
set(pioasm_HINT ${USERHOME}/.pico-sdk/tools/2.0.0/pioasm)
if(EXISTS ${pioasm_HINT})
    set(pioasm_DIR ${pioasm_HINT})
endif()
set(picotool_HINT ${USERHOME}/.pico-sdk/picotool/2.0.0/picotool)
if(EXISTS ${picotool_HINT})
    set(picotool_DIR ${picotool_HINT})
endif()
if(PICO_TOOLCHAIN_PATH MATCHES "RISCV")
    set(PICO_PLATFORM rp2350-riscv CACHE STRING "Pico Platform")
    if(PICO_TOOLCHAIN_PATH MATCHES "COREV")
        set(PICO_COMPILER pico_riscv_gcc_zcb_zcmp)
    endif()
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(midi2usbdev C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(PROJECT midi-multstream2usbdev)

project(${PROJECT})
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pio_midi_uart_lib)

add_executable(${PROJECT}
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
  ${CMAKE_CURRENT_LIST_DIR}/midi_device_multistream.c
)

target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/preprocessor/include
)

target_link_options(${PROJECT} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${PROJECT} PRIVATE -Wall -Wextra -DCFG_TUSB_DEBUG=1)
target_link_libraries(${PROJECT} pio_midi_uart_lib tinyusb_device tinyusb_board pico_stdlib pico_unique_id)

pico_add_extra_outputs(${PROJECT})
