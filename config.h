#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
// ============================================================================
// Pin Assignments
// ============================================================================

// UART0 for MIDI input (31,250 baud)
#define MIDI_UART_ID uart0
#define MIDI_TX_PIN 0 // Reserved but unused
#define MIDI_RX_PIN 1 // MIDI input from opto-isolator

// WS2812B LED data output (PIO0, SM0)
#define LED_PIN 2

// Reset Button (Active Low, Pull-Up)
#define RESET_BTN_PIN 3
#define RESET_BUTTON_FLASH_TIME 250

// Stacked Layout:
// Panel 1 (Top): 32x8
// Panel 2 (Bottom): 32x8
// Total: 32x16
#define PANEL_WIDTH 32
#define PANEL_HEIGHT 16
#define LED_COUNT (PANEL_WIDTH * PANEL_HEIGHT) // 512 LEDs

// Global brightness (0-255).
extern uint8_t global_brightness;

// Potentiometer Configuration
#define POT_PIN 26
#define POT_ADC_NUM 0          // ADC0 is on GPIO26
#define ENABLE_POTENTIOMETER 1 // Set to 0 if no pot connected

// ============================================================================
// MIDI Configuration
// ============================================================================

#define MIDI_BAUD_RATE 31250
#define MAX_CHANNELS 16
#define MAX_NOTES 128

#endif // CONFIG_H
