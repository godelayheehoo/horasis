# MIDI LED Grid Visualizer

A responsive, high-performance LED grid visualizer powered by a Raspberry Pi Pico 2 (RP2350). This project translates real-time MIDI input into dynamic, 2D tiled visualizations across a 32x16 WS2812B LED matrix.

## Features

- **Dynamic 2D Layout**: Uses a Binary Space Partitioning (BSP) algorithm to intelligently divide the display.
    - **Channel Tiling**: The screen is split vertically/horizontally based on the number of active MIDI channels.
    - **Note Tiling**: Each channel's region is further subdivided based on the number of unique notes played since the last reset.
- **Color Mapping**: Each of the 16 MIDI channels is assigned a unique, vibrant color for easy identification.
- **Hardware Validated**: Built for the Raspberry Pi Pico 2 using the C/C++ SDK for maximum performance.
- **Reset Functionality**: Dedicated hardware button to clear the layout and start fresh.
- **Diagnostic Output**: USB Serial debugging for monitoring MIDI events and layout calculations.

## Hardware Setup

### Core Components
- **Microcontroller**: Raspberry Pi Pico 2 (RP2350)
- **LED Matrix**: Two 8x32 WS2812B panels stacked vertically to form a 32x16 grid (512 LEDs total).
- **MIDI Input**: Standard MIDI 5-pin DIN connector via an Optocoupler circuit (e.g., 6N138) to UART.

### Pinout Configuration

| Component | Pico Pin | Description |
|-----------|----------|-------------|
| **LED Data** | GPIO 22 | WS2812B Data Line (Level shifted to 5V recommended) |
| **MIDI RX**  | GPIO 1   | UART0 RX (Connected to Optocoupler output) |
| **Reset Btn**| GPIO 3   | Active Low button (connect to GND when pressed) |
| **Power**    | VBUS/VSYS| 5V Power for LEDs (External supply recommended for 512 LEDs) |

### Physical Layout

The code is configured for a specific 32x16 physical layout composed of two 8x32 panels:
1.  **Top Panel (Rows 0-7)**: Physically mounted with the first LED at the Top-Right.
2.  **Bottom Panel (Rows 8-15)**: Physically mounted with the first LED at the Top-Left.
See `leds.cpp` for the exact serpentine mapping logic.

## Building the Project

### Prerequisites
- Raspberry Pi Pico SDK installed and configured.
- `cmake` and `arm-none-eabi-gcc` toolchain.

### Build Commands

```bash
mkdir build
cd build
cmake -DPICO_PLATFORM=rp2350-arm-s ..
cmake --build .
```

### Flashing
Hold the BOOTSEL button on the Pico 2, plug it in via USB, and drag the generated `midi_leds.uf2` file onto the mass storage device.

## Software Architecture

- **`main.cpp`**: Core rendering loop. Runs at ~60FPS, handling MIDI polling and LED updating.
- **`layout.cpp`**: Implements the recursive BSP tiling algorithm. Manages the state of `Rect` regions for channels and notes.
- **`leds.cpp`**: Handles the raw pixel mapping and WS2812B communication via PIO and DMA.
- **`midi.cpp`**: UART-based MIDI parser with a state machine for handling Note On/Off messages.

## License

[MIT License](LICENSE)
