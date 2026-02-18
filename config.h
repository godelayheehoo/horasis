#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Pin Assignments
// ============================================================================

// UART0 for MIDI input (31,250 baud)
#define MIDI_UART_ID uart0
#define MIDI_TX_PIN 0 // Reserved but unused
#define MIDI_RX_PIN 1 // MIDI input from opto-isolator

// WS2812B LED data output (PIO0, SM0)
#define LED_PIN 2

// ============================================================================
// LED Panel Configuration
// ============================================================================

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 8
#define LED_COUNT (PANEL_WIDTH * PANEL_HEIGHT) // 512 LEDs

// ============================================================================
// MIDI Configuration
// ============================================================================

#define MIDI_BAUD_RATE 31250
#define MAX_CHANNELS 16
#define MAX_NOTES 128

#endif // CONFIG_H
