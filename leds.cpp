#include "leds.h"
#include "config.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

// Internal framebuffer: 512 LEDs × 4 bytes (GRB + padding) = 2KB
static uint32_t framebuffer[LED_COUNT];
static PIO pio = pio0;
static uint sm = 0;
static int dma_chan;

// ============================================================================
// Coordinate Mapping
// ============================================================================

// Convert logical (x, y) to physical LED index
// Assumes serpentine wiring: even rows left-to-right, odd rows right-to-left
// NOTE: This may need adjustment when hardware arrives!
// 64x8 grid made of two 8x32 panels.
//
// Panel 1 (Left, x=0..31):
//   Starts at Top-Right (x=31, y=0).
//   Snakes Left.
//   Col 31 goes DOWN. Col 30 goes UP.
//
// Panel 2 (Right, x=32..63):
//   Starts at Top-Left (x=32, y=0).
//   Snakes Right.
//   Col 32 goes DOWN. Col 33 goes UP.
// 32x16 Grid (Two 8x32 Panels Stacked)
// Vertical Column-Major Wiring
//
// Panel 1 (Top, y=0..7):
//   Logical x=0..31.
//   Physically starts at Top-Right, fills Leftwards.
//   Indices 0..255.
//
// Panel 2 (Bottom, y=8..15):
//   Logical x=0..31.
//   Physically starts at Top-Left, fills Rightwards.
//   Indices 256..511.

static int xyToIndex(int x, int y) {
  if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
    return -1;
  }

  int panel_base = 0;
  int local_y = y;

  // Check which panel (Top or Bottom)
  if (y < 8) {
    // === PANEL 1 (Top) ===
    // Indices 0..255
    // Maps x=0..31 to Physical Cols 31..0
    panel_base = 0;
    local_y = y;

    // Physical Column (31 - x)
    int col_idx = 31 - x;
    int base = panel_base + (col_idx * 8);

    // Serpentine
    if (col_idx % 2 == 0) {
      return base + local_y; // Down
    } else {
      return base + (7 - local_y); // Up
    }
  } else {
    // === PANEL 2 (Bottom) ===
    // Indices 256..511
    // Maps x=0..31 to Physical Cols 0..31
    panel_base = 256;
    local_y = y - 8; // Offset y to 0..7

    // Physical Column (x)
    int col_idx = x;
    int base = panel_base + (col_idx * 8);

    // Serpentine
    if (col_idx % 2 == 0) {
      return base + local_y; // Down
    } else {
      return base + (7 - local_y); // Up
    }
  }
}

// ... Public API ...

void leds_init() {
  // Clear framebuffer
  for (int i = 0; i < LED_COUNT; i++) {
    framebuffer[i] = 0;
  }

  // Load WS2812 PIO program
  uint offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);

  // Set up DMA channel for efficient transfers
  dma_chan = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

  dma_channel_configure(dma_chan, &c,
                        &pio->txf[sm], // Write to PIO TX FIFO
                        framebuffer,   // Read from framebuffer
                        LED_COUNT,     // Transfer count
                        false          // Don't start yet
  );
}

void leds_setPixel(int x, int y, uint32_t rgb) {
  // Input RGB: 0x00RRGGBB
  int idx = xyToIndex(x, y);
  if (idx >= 0 && idx < LED_COUNT) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    // Scale by brightness
    r = (r * LED_BRIGHTNESS) >> 8;
    g = (g * LED_BRIGHTNESS) >> 8;
    b = (b * LED_BRIGHTNESS) >> 8;

    // Convert to GRB for WS2812 (0x00GGRRBB)
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    framebuffer[idx] = grb << 8;
  }
}

void leds_show() {
  // Trigger DMA transfer
  dma_channel_set_read_addr(dma_chan, framebuffer, true);

  // Wait for transfer to complete
  dma_channel_wait_for_finish_blocking(dma_chan);
}

void leds_clear() {
  for (int i = 0; i < LED_COUNT; i++) {
    framebuffer[i] = 0;
  }
}

void leds_startup_sequence() {
  // Debug Sequence: Red -> Green -> Blue -> Cyan
  uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0x00FFFF};

  for (int i = 0; i < 4; i++) {
    uint32_t c = colors[i];
    leds_clear();
    for (int j = 0; j < LED_COUNT; j++) {
      leds_setPixel(j % PANEL_WIDTH, j / PANEL_WIDTH, c);
    }
    leds_show();
    sleep_ms(500);
  }

  leds_clear();
  leds_show();
}
