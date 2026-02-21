#include "config.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "layout.h"
#include "leds.h"
#include "midi.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstdlib>

// ============================================================================
// MIDI Callbacks
// ============================================================================

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  (void)velocity; // Not using velocity for brightness (future enhancement)

  // Register channel and note if first time seen
  registerChannel(channel);
  registerNote(channel, note);

  printf("NoteOn: Ch=%d Note=%d Vel=%d (Active Ch: %d)\n", channel, note,
         velocity, activeChannelCount); // DEBUG

  // Set note active
  setNoteActive(channel, note, true);
}

void onNoteOff(uint8_t channel, uint8_t note) {
  // Set note inactive (no need to register if not already seen)
  if (channel < MAX_CHANNELS && note < MAX_NOTES) {
    setNoteActive(channel, note, false);
  }
}

// ============================================================================
// Render Loop
// ============================================================================

void render() {
  leds_clear();

  // Iterate through all channels
  for (int c = 0; c < MAX_CHANNELS; c++) {
    if (!channels[c].seen)
      continue;

    uint32_t color = channels[c].color;

    // Iterate through all notes in this channel
    for (int n = 0; n < MAX_NOTES; n++) {
      NoteEntry &ne = channels[c].notes[n];

      // Skip if note not seen, not active, or has empty bounds
      if (!ne.seen || !ne.active || ne.bounds.w == 0 || ne.bounds.h == 0) {
        continue;
      }

      // Light up 2D Rect regions which are now variable size
      // Check for valid dimensions
      if (ne.bounds.w == 0 || ne.bounds.h == 0)
        continue;

      for (int x = ne.bounds.x; x < ne.bounds.x + ne.bounds.w; x++) {
        for (int y = ne.bounds.y; y < ne.bounds.y + ne.bounds.h; y++) {
          leds_setPixel(x, y, color);
        }
      }
    }
  }

  leds_show();
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
  stdio_init_all();
  printf("MidiLeds Booting...\n");

  // Initialize all subsystems
  leds_init();
  leds_startup_sequence();
  midi_init();
  layout_init();

// Initialize onboard LED for heartbeat (best effort)
#ifdef PICO_DEFAULT_LED_PIN
  const uint LED_PIN_ONBOARD = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN_ONBOARD);
  gpio_set_dir(LED_PIN_ONBOARD, GPIO_OUT);
#endif

  // Initialize Reset Button
  gpio_init(RESET_BTN_PIN);
  gpio_set_dir(RESET_BTN_PIN, GPIO_IN);
  gpio_pull_up(RESET_BTN_PIN);

#if ENABLE_POTENTIOMETER
  adc_init();
  adc_gpio_init(POT_PIN);
  adc_select_input(POT_ADC_NUM);
  printf("Potentiometer Enabled on Pin %d (ADC %d)\n", POT_PIN, POT_ADC_NUM);
#endif

  // Main render loop
  while (true) {
    // Limit frame rate to ~60 FPS (16ms)
    // WS2812B timing is sensitive; flooding it might cause issues
    static uint32_t last_frame = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_frame >= 16) {
#if ENABLE_POTENTIOMETER
      uint16_t adc_val = adc_read();
      global_brightness = adc_val >> 4; // Map 12-bit (0-4095) to 8-bit (0-255)
#endif

      // Critical Section: Disable interrupts during transmission to prevent
      // timing glitches
      uint32_t irq_status = save_and_disable_interrupts();
      render();
      restore_interrupts(irq_status);
      last_frame = now;
    }

    midi_poll(); // Poll as fast as possible to drain FIFO

    // Heartbeat: Blink onboard LED every 500ms
#ifdef PICO_DEFAULT_LED_PIN
    static uint32_t last_blink = 0;
    if (to_ms_since_boot(get_absolute_time()) - last_blink > 500) {
      gpio_xor_mask(1u << LED_PIN_ONBOARD);
      last_blink = to_ms_since_boot(get_absolute_time());
    }
#endif

    // Poll Reset Button (Active Low)
    if (!gpio_get(RESET_BTN_PIN)) {
      printf("Reset Button Pressed!\n");
      // Debounce: Wait for release
      while (!gpio_get(RESET_BTN_PIN)) {
        sleep_ms(10);
      }
      layout_reset();
      
      // Flash random colors
      for (int y = 0; y < PANEL_HEIGHT; y++) {
        for (int x = 0; x < PANEL_WIDTH; x++) {
          uint32_t color = ((rand() % 128) << 16) | ((rand() % 128) << 8) | (rand() % 128);
          leds_setPixel(x, y, color);
        }
      }
      leds_show();
      sleep_ms(RESET_BUTTON_FLASH_TIME); // Visual feedback

      leds_clear();
      leds_show();
    }
  }

  return 0;
}
