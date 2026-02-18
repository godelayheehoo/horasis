# MIDI LED Matrix — Implementation Plan

**Target:** Raspberry Pi Pico | C++ / Pico SDK | WS2812B 16×32 Matrix | UART MIDI

---

## 1. Project Overview

This project receives MIDI data via UART (DIN-5 connector) on a Raspberry Pi Pico and drives a 16×32 WS2812B LED matrix panel in real time. The display is fully dynamic: the grid is subdivided among active MIDI channels, and within each channel's region it is further subdivided among the notes that have been seen on that channel. When a note-on message is received the corresponding sub-region lights up; it goes dark on note-off.

Channels and notes are permanent once seen — they are never removed from the layout until the device is reset. Layout is recomputed immediately whenever a new channel or note is first encountered. The visual result of a mid-note reflow is an acceptable instantaneous jump; animated transitions are out of scope for the initial implementation.

---

## 2. Hardware

### 2.1 Platform

Raspberry Pi Pico (RP2040). 264 KB SRAM, dual-core Cortex-M0+, PIO hardware for deterministic signal generation. All code runs on core 0; core 1 is reserved for future use.

### 2.2 LED Panel

Two WS2812B 8×32 panels connected in series, forming a single logical 16×32 grid (512 LEDs total). The panels are addressed as one contiguous chain; the second panel's data-in connects to the first panel's data-out.

### 2.3 MIDI Input

Standard MIDI over DIN-5, delivered at 31,250 baud. A 6N138 opto-isolator circuit (or equivalent MIDI input circuit) converts the DIN-5 signal to 3.3 V UART logic and feeds the Pico's UART0 RX pin (GP1). The MIDI spec requires the opto-isolator to be powered from the receiving device, not the sending device; use the standard resistor/diode protection circuit.

### 2.4 Pin Assignments

Use the following default pin assignments. These can be changed by editing the constants at the top of `config.h`.

```
GP0  — UART0 TX  (unused for MIDI but reserved)
GP1  — UART0 RX  (MIDI input)
GP2  — PIO0 SM0  (WS2812B data out to LED chain)
VSYS — 5V power for LED panels (via adequate supply)
GND  — Common ground
```

> **Important:** WS2812B panels at full brightness draw up to 60 mA per LED. At 512 LEDs that is ~30 A theoretical maximum. In practice this design will never illuminate all LEDs white simultaneously, but use an external 5 V supply rated for at least 5 A and do not power the panels from the Pico's onboard regulator.

---

## 3. Software Architecture

The firmware is written in C++ using the Pico SDK (cmake build system). It is organized into four layers with clean separation of concerns:

```
┌─────────────────────────────────────────┐
│           main.cpp (render loop)        │
├─────────────┬───────────────────────────┤
│ midi.h/.cpp │  layout.h/.cpp            │
│  (parser)   │  (space partitioner)      │
├─────────────┴───────────────────────────┤
│           leds.h/.cpp                   │
│  (PIO driver + coordinate mapping)      │
└─────────────────────────────────────────┘
```

### 3.1 LED Driver — `leds.h` / `leds.cpp`

Wraps the PIO-based WS2812B driver from the Pico SDK examples. Responsibilities:

- Initialise PIO state machine and DMA channel on startup.
- **`setPixel(int x, int y, uint32_t grb)`** — the only function the rest of the codebase calls to write a color.
- **`show()`** — flushes the internal framebuffer to the LED chain via DMA.
- **`clear()`** — fills the framebuffer with zeros.

**Coordinate mapping.** WS2812B panels are wired in a serpentine (boustrophedon) pattern. The function below converts logical (x, y) coordinates to the physical LED index. This will almost certainly need a one-time adjustment after the hardware arrives; the serpentine direction and panel join point are the two things most likely to need changing.

```cpp
#define PANEL_WIDTH  32
#define PANEL_HEIGHT 16

int xyToIndex(int x, int y) {
    // Even rows: left to right. Odd rows: right to left.
    if (y % 2 == 0)
        return y * PANEL_WIDTH + x;
    else
        return y * PANEL_WIDTH + (PANEL_WIDTH - 1 - x);
}
```

To test the mapping: write a diagnostic loop on startup that lights LED index 0 red, index 1 green, index 2 blue. Observe which physical LEDs light and adjust the formula accordingly.

### 3.2 MIDI Parser — `midi.h` / `midi.cpp`

Reads bytes from UART0 using the Pico SDK's non-blocking `uart_getc` (polled in the main loop — no interrupt handler required at this latency target). Implements a minimal running-status state machine covering the three message types this project needs:

- **Note On** (status `0x9n`, two data bytes: note 0–127, velocity 0–127)
- **Note Off** (status `0x8n`, two data bytes: note 0–127, velocity)
- **All Notes Off** (CC `0x7B` on channel n — treat as note-off for all active notes on that channel)

All other message types (Program Change, Pitch Bend, SysEx, etc.) are silently discarded. SysEx must be handled by consuming bytes until `0xF7` is seen, to avoid corrupting the running status.

The parser fires callbacks rather than returning values, keeping the call site clean:

```cpp
// Implement these callbacks in main.cpp
void onNoteOn (uint8_t channel, uint8_t note, uint8_t velocity);
void onNoteOff(uint8_t channel, uint8_t note);
```

Note On with velocity 0 must be treated as Note Off (this is standard MIDI running-status convention).

### 3.3 Layout Engine — `layout.h` / `layout.cpp`

This is the core of the project. It maintains state for all channels and notes, computes rectangular regions, and answers queries about which LEDs should be lit.

#### 3.3.1 Data Structures

```cpp
#define MAX_CHANNELS 16
#define MAX_NOTES    128   // full MIDI range
#define LED_COUNT    512   // 16 * 32

struct NoteEntry {
    bool seen;        // has this note ever fired on this channel?
    bool active;      // is a note-on currently held?
    int  regionStart; // first LED index in this note's sub-region
    int  regionSize;  // number of LEDs in this note's sub-region
};

struct ChannelEntry {
    bool      seen;              // has this channel been detected?
    uint32_t  color;             // GRB color assigned at first detection
    int       regionStart;       // first LED index of channel's region
    int       regionSize;        // number of LEDs in channel's region
    int       seenNoteCount;     // how many distinct notes seen so far
    NoteEntry notes[MAX_NOTES];
};

extern ChannelEntry channels[MAX_CHANNELS];
extern int          activeChannelCount;
```

#### 3.3.2 Region Assignment Strategy

Regions are contiguous linear slices of the LED index space (row-major through the panel). This is the simplest strategy that produces correct results; the visual layout naturally fills the panel row by row.

When a new channel is registered, the 512 LED indices are divided evenly among all currently-known channels, with remainder LEDs distributed one each to the earliest channels. Each channel's region is then similarly subdivided among its known notes.

This means every new channel or note triggers a full recompute of all region boundaries. The recompute is O(channels × notes) which is fast enough on the Pico to be done synchronously on the note-on/channel-detect event.

#### 3.3.3 Reflow Algorithm

```cpp
void recomputeLayout() {
    int totalLEDs = LED_COUNT;
    int chCount   = activeChannelCount;
    int base      = totalLEDs / chCount;
    int remainder = totalLEDs % chCount;
    int cursor    = 0;

    int ci = 0;  // index into active channels
    for (int c = 0; c < MAX_CHANNELS; c++) {
        if (!channels[c].seen) continue;
        int size = base + (ci < remainder ? 1 : 0);
        channels[c].regionStart = cursor;
        channels[c].regionSize  = size;
        cursor += size;
        recomputeNoteLayout(c);  // subdivide within channel
        ci++;
    }
}

void recomputeNoteLayout(int c) {
    ChannelEntry &ch = channels[c];
    int nCount   = ch.seenNoteCount;
    if (nCount == 0) return;
    int base      = ch.regionSize / nCount;
    int remainder = ch.regionSize % nCount;
    int cursor    = ch.regionStart;
    int ni        = 0;
    for (int n = 0; n < MAX_NOTES; n++) {
        if (!ch.notes[n].seen) continue;
        int size = base + (ni < remainder ? 1 : 0);
        ch.notes[n].regionStart = cursor;
        ch.notes[n].regionSize  = size;
        cursor += size;
        ni++;
    }
}
```

#### 3.3.4 Oversubscription Handling

If a channel has more seen notes than it has LEDs in its region, some notes will receive a `regionSize` of 0. This is handled as follows:

- Notes are assigned region sizes in the order they were first seen (lowest MIDI note number among ties, since note numbers are iterated in order). The first N notes receive at least 1 LED; notes beyond N receive `regionSize = 0`.
- When rendering, any note with `regionSize = 0` is silently skipped — it does not light up, but its active state is still tracked correctly so note-off messages are still processed.
- This situation is expected to be very rare (requires more unique notes on one channel than that channel's LED allocation, e.g. 33+ distinct notes on one of 16 channels giving it only 32 LEDs).
- No notes are ever dropped or evicted. Once seen, a note remains in the layout permanently.

### 3.4 Color Assignment

Channels are assigned colors at first detection using 16 evenly-spaced hues around the HSV color wheel, converted to GRB for WS2812B. The mapping is deterministic by channel number (channel 1 always gets the same hue), so the colors are stable regardless of the order channels are detected. Saturation is fixed at 100%, value at 80% (not full brightness, to reduce current draw and improve visual quality).

```cpp
// Precomputed GRB values for channels 0–15
// Hue steps of 22.5°, S=100%, V=80%
// NOTE: WS2812B byte order is G, R, B (not R, G, B)
// Encode as 0xGGRRBB in the 24 low bits of a uint32_t
// Verify byte order empirically when hardware is in hand — some clones differ.
const uint32_t CHANNEL_COLORS[16] = {
    0xCC0000, // ch 0  red
    0xCC6600, // ch 1  orange
    0xCCCC00, // ch 2  yellow
    0x66CC00, // ch 3  yellow-green
    0x00CC00, // ch 4  green
    0x00CC66, // ch 5  spring green
    0x00CCCC, // ch 6  cyan
    0x0066CC, // ch 7  azure
    0x0000CC, // ch 8  blue
    0x6600CC, // ch 9  violet
    0xCC00CC, // ch 10 magenta
    0xCC0066, // ch 11 rose
    0x996600, // ch 12 warm brown
    0x009966, // ch 13 teal
    0x990099, // ch 14 purple
    0x669999, // ch 15 slate
};
```

### 3.5 Main Render Loop — `main.cpp`

```cpp
int main() {
    leds_init();    // PIO + DMA
    midi_init();    // UART0 at 31250 baud
    layout_init();  // zero all state

    while (true) {
        midi_poll();   // read available bytes, fire callbacks
        render();      // redraw framebuffer, call show()
    }
}

void render() {
    leds_clear();
    for (int c = 0; c < MAX_CHANNELS; c++) {
        if (!channels[c].seen) continue;
        for (int n = 0; n < MAX_NOTES; n++) {
            NoteEntry &ne = channels[c].notes[n];
            if (!ne.seen || !ne.active || ne.regionSize == 0) continue;
            uint32_t color = channels[c].color;
            for (int i = ne.regionStart; i < ne.regionStart + ne.regionSize; i++) {
                int x = i % PANEL_WIDTH;
                int y = i / PANEL_WIDTH;
                leds_setPixel(x, y, color);
            }
        }
    }
    leds_show();
}
```

The render loop runs as fast as the Pico allows. The WS2812B protocol requires ~30 µs per LED (512 LEDs ≈ 15 ms per full frame), giving a natural cap of ~60 fps — more than sufficient.

---

## 4. Project File Structure

```
led_grid/
├── CMakeLists.txt
├── pico_sdk_import.cmake
├── config.h          ← pin assignments, panel dimensions, constants
├── main.cpp          ← render loop + MIDI callbacks
├── leds.h / leds.cpp ← PIO driver, setPixel, show, clear
├── midi.h / midi.cpp ← UART parser, callbacks
├── layout.h          ← data structures, extern declarations
└── layout.cpp        ← recomputeLayout, recomputeNoteLayout, layout_init
```

---

## 5. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(midi_leds C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(midi_leds
    main.cpp leds.cpp midi.cpp layout.cpp
)

# Pull in WS2812 PIO program from SDK examples
pico_generate_pio_header(midi_leds
    ${PICO_SDK_PATH}/src/rp2_common/hardware_pio/pio_programs/ws2812.pio
)

target_link_libraries(midi_leds
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_uart
)

pico_add_extra_outputs(midi_leds)  # generates .uf2 for drag-drop flash
```

---

## 6. Recommended Bring-Up Order

Build and test one layer at a time. Do not attempt to integrate everything at once.

1. **LED driver only.** Implement `leds.h`/`leds.cpp`. Write a test main that lights pixel (0,0) red, (1,0) green, (0,1) blue. Verify coordinates are correct. Fix `xyToIndex` if needed.

2. **Coordinate map sweep.** Loop through all 512 LEDs, light each one in sequence with a short delay. Visually confirm the serpentine path is correct and the panel boundaries are seamless.

3. **MIDI parser only.** Connect MIDI source. Print parsed note-on/off messages over USB serial (`pico_enable_stdio_usb` in CMakeLists). Verify channel, note, and velocity values are correct before touching the LED code.

4. **Layout engine, static test.** Call layout functions directly from main with hardcoded channel/note registrations. Verify region boundaries look correct by lighting those LED ranges. No MIDI yet.

5. **Full integration.** Wire MIDI callbacks to layout engine. Play notes and verify the display responds correctly.

---

## 7. Known Limitations & Future Work

- **`xyToIndex` may need adjustment.** The serpentine direction and panel join point can only be verified with physical hardware. The function is isolated in `leds.cpp` for easy editing.
- **No velocity sensitivity.** All note-on events light the region at full channel color regardless of velocity. Velocity-to-brightness mapping is a straightforward future addition.
- **No hue variation within a channel.** All notes in a channel share the same color. Per-note hue offset is a future addition.
- **Rectangular regions only.** The row-major linear assignment produces non-rectangular regions at channel/note count boundaries. A BSP (binary space partitioning) layout engine that produces true rectangles is a well-defined future upgrade.
- **No animated transitions on reflow.** Layout changes are instantaneous. A crossfade can be added later without structural changes, since the render loop is already stateless.
- **WS2812B byte order.** Verify GRB vs RGB byte order empirically; some clones differ.
