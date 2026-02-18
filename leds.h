#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>

// Initialize the LED driver (PIO + DMA)
void leds_init();

// Set a single pixel color at logical (x, y) coordinates
// Color format: 0x00GGRRBB (24-bit GRB for WS2812B)
void leds_setPixel(int x, int y, uint32_t grb);

// Flush the framebuffer to the LED chain via DMA
void leds_show();

// Clear all pixels to black (does not auto-flush)
void leds_clear();

// Run startup diagnostics (moving cyan square)
void leds_startup_sequence();

#endif // LEDS_H
