#include "config.h"
#include "layout.h"
#include "leds.h"
#include "midi.h"
#include "pico/stdlib.h"

// ============================================================================
// MIDI Callbacks
// ============================================================================

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  (void)velocity; // Not using velocity for brightness (future enhancement)

  // Register channel and note if first time seen
  registerChannel(channel);
  registerNote(channel, note);

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

      // Skip if note not seen, not active, or oversubscribed (regionSize = 0)
      if (!ne.seen || !ne.active || ne.regionSize == 0) {
        continue;
      }

      // Light up all LEDs in this note's region
      for (int i = ne.regionStart; i < ne.regionStart + ne.regionSize; i++) {
        int x = i % PANEL_WIDTH;
        int y = i / PANEL_WIDTH;
        leds_setPixel(x, y, color);
      }
    }
  }

  leds_show();
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
  // Initialize all subsystems
  leds_init();
  leds_startup_sequence();
  midi_init();
  layout_init();

  // Main render loop
  while (true) {
    midi_poll(); // Read MIDI bytes, fire callbacks
    render();    // Redraw LED matrix
  }

  return 0;
}
