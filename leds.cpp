#include "leds.h"
#include "config.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

// Internal framebuffer: LED_COUNT LEDs × 4 bytes (GRB + padding)
static uint32_t framebuffer[LED_COUNT];
static PIO pio = pio0;
static uint sm = 0;
static int dma_chan;

// Global brightness
uint8_t global_brightness = 128;

// ============================================================================
// Coordinate Mapping
// ============================================================================

// Convert logical (x, y) to physical LED index
// Assumes serpentine wiring: even rows left-to-right, odd rows right-to-left
// NOTE: This may need adjustment based on specific hardware!
// Stacked Layout:
// Multiple 32x8 panels stacked vertically.
// Total dimensions: 32 x PANEL_HEIGHT
//
// Generalized Multi-Panel Stack (32x8 panels)
// Vertical Column-Major Serpentine Wiring
//
// Panels are stacked vertically. Even panels (0, 2, ...) start Top-Right
// and snake Left. Odd panels (1, 3, ...) start Top-Left and snake Right.
// This allows for continuous data chaining (DO -> DI) between panels.

static int xyToIndex(int x, int y) {
  if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
    return -1;
  }

  const int PANEL_H = 8; // Each physical panel is 8 pixels high
  int panel_idx = y / PANEL_H;
  int local_y = y % PANEL_H;
  int panel_base = panel_idx * (PANEL_WIDTH * PANEL_H);

  int col_idx;
  // Panels snake: Even panels (0, 2, ...) flow Right-to-Left, 
  // Odd panels (1, 3, ...) flow Left-to-Right.
  if (panel_idx % 2 == 0) {
    col_idx = (PANEL_WIDTH - 1) - x;
  } else {
    col_idx = x;
  }

  int base = panel_base + (col_idx * PANEL_H);

  // Serpentine wiring within the columns of a single panel
  if (col_idx % 2 == 0) {
    return base + local_y; // Down
  } else {
    return base + (PANEL_H - 1 - local_y); // Up
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
    r = (r * global_brightness) >> 8;
    g = (g * global_brightness) >> 8;
    b = (b * global_brightness) >> 8;

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
