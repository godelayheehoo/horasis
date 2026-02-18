cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(midi_leds C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(midi_leds
    main.cpp leds.cpp midi.cpp layout.cpp
)

pico_generate_pio_header(midi_leds
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

target_link_libraries(midi_leds
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_uart
)

pico_add_extra_outputs(midi_leds)	