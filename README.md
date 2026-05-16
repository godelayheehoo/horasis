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

The code supports a flexible number of 32x8 physical panels stacked vertically. 
- **Configuration**: Adjust `PANEL_HEIGHT` in `config.h` (must be a multiple of 8).
- **Mapping**: The software automatically handles a "snaking" vertical layout where even-indexed panels flow Right-to-Left and odd-indexed panels flow Left-to-Right.
- **Internal Wiring**: Each panel is assumed to have serpentine column wiring.

## Performance & Scalability

As the number of LEDs increases, the time required to transmit data also increases linearly (~30µs per LED).

| Grid Size | Total LEDs | Update Time | Max Refresh Rate |
|-----------|------------|-------------|------------------|
| 32 x 16   | 512        | ~15.4 ms    | ~65 FPS          |
| 32 x 32   | 1024       | ~30.7 ms    | ~32 FPS          |
| 32 x 64   | 2048       | ~61.4 ms    | ~16 FPS          |

> [!WARNING]
> **MIDI Buffer Overflow Risk**: Because the current LED driver is "blocking," high LED counts (e.g., 32x64) will prevent the CPU from polling MIDI for long periods. If a burst of MIDI messages arrives during an update, the hardware UART buffer (32 bytes) may overflow, leading to lost notes. For very large grids, it is recommended to move the rendering to the second CPU core.

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
