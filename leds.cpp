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
static int xyToIndex(int x, int y) {
  if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
    return -1; // Out of bounds
  }

  if (x < 32) {
    // === PANEL 1 (Left) ===
    // Columns processed: 31, 30, ..., 0
    int col_from_start = 31 - x;
    int base = col_from_start * 8; // 8 rows

    if (col_from_start % 2 == 0) {
      // Even columns (31, 29, ...): Down
      return base + y;
    } else {
      // Odd columns (30, 28, ...): Up
      return base + (7 - y);
    }
  } else {
    // === PANEL 2 (Right) ===
    // Starts after 256 LEDs
    int base_panel2 = 256;
    // Columns processed: 32, 33, ..., 63
    int col_from_start = x - 32;
    int base = base_panel2 + (col_from_start * 8);

    if (col_from_start % 2 == 0) {
      // Even columns (32, 34, ...): Down
      return base + y;
    } else {
      // Odd columns (33, 35, ...): Up
      return base + (7 - y);
    }
  }
}

// ============================================================================
// Public API
// ============================================================================

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

void leds_setPixel(int x, int y, uint32_t grb) {
  int idx = xyToIndex(x, y);
  if (idx >= 0 && idx < LED_COUNT) {
    framebuffer[idx] = grb;
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
  // Cyan: 0x00CCCC (G=CC, R=00, B=CC)
  // WS2812B expects GRB format.
  // 0xCC (Green) 0x00 (Red) 0xCC (Blue) -> 0xCC00CC in uint32_t 0xGGRRBB?
  // Wait, config.h says:
  // 0x00CCCC, // ch 6  cyan (R=00, G=CC, B=CC)
  // But leds_setPixel takes GRB?
  // "Encode as 0xGGRRBB in the 24 low bits"
  // So Cyan (R=00, G=CC, B=CC) -> G=CC, R=00, B=CC -> 0xCC00CC.

  uint32_t color = 0xCC00CC; // Cyan in 0xGGRRBB format

  for (int x = 0; x < PANEL_WIDTH; x += 8) {
    leds_clear();

    // Draw 8x8 square
    for (int dx = 0; dx < 8; dx++) {
      for (int dy = 0; dy < 8; dy++) {
        leds_setPixel(x + dx, dy, color);
      }
    }

    leds_show();
    sleep_ms(200);
  }

  leds_clear();
  leds_show();
}
