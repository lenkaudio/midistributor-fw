# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(midistributor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(PROJECT midistributor)

#project(${PROJECT})
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pio_midi_uart_lib)
set_property(TARGET pio_midi_uart_lib APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS PIO_MIDI_UART_TX_NOT_BUFFERED=1)

#add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/lwjson/lwjson)

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
target_link_libraries(${PROJECT} 
                      #lwjson
                      pico_stdlib
                      pico_unique_id
                      pio_midi_uart_lib 
                      tinyusb_device 
                      tinyusb_board)

pico_add_extra_outputs(${PROJECT})

target_compile_definitions(${PROJECT} PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=28
  PICO_DEFAULT_UART_RX_PIN=29
)