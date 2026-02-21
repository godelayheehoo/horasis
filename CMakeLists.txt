cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(midi_leds C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(PICO_PLATFORM rp2350-arm-s)
pico_sdk_init()



add_executable(midi_leds
    main.cpp leds.cpp midi.cpp layout.cpp
)

# Enable USB stdio for debug output
pico_enable_stdio_usb(midi_leds 1)
pico_enable_stdio_uart(midi_leds 0)

pico_generate_pio_header(midi_leds
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

target_link_libraries(midi_leds
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_uart
    hardware_sync
    hardware_adc
)

pico_add_extra_outputs(midi_leds)	