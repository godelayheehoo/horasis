#include "layout.h"
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

ChannelEntry channels[MAX_CHANNELS];
int activeChannelCount = 0;

// ============================================================================
// Color Palette
// ============================================================================

// Precomputed GRB values for 16 channels
// Hue steps of 22.5°, S=100%, V=80%
// NOTE: WS2812B byte order is G, R, B (encoded as 0x00GGRRBB)
// Verify byte order empirically when hardware arrives!
static const uint32_t CHANNEL_COLORS[16] = {
    0x00CC0000, // ch 0  red
    0x66CC00,   // ch 1  orange
    0xCC6600,   // ch 2  yellow
    0xCC0066,   // ch 3  yellow-green
    0x00CC00,   // ch 4  green
    0x00CC66,   // ch 5  spring green
    0xCC00CC,   // ch 6  cyan
    0xCC0066,   // ch 7  azure
    0x0000CC,   // ch 8  blue
    0x6600CC,   // ch 9  violet
    0xCC00CC,   // ch 10 magenta
    0x660066,   // ch 11 rose
    0x336600,   // ch 12 warm brown
    0x003366,   // ch 13 teal
    0x330033,   // ch 14 purple
    0x336666,   // ch 15 slate
};

// ============================================================================
// Internal Functions
// ============================================================================

static void recomputeNoteLayout(int c) {
  ChannelEntry &ch = channels[c];
  int nCount = ch.seenNoteCount;

  if (nCount == 0) {
    return;
  }

  int base = ch.regionSize / nCount;
  int remainder = ch.regionSize % nCount;
  int cursor = ch.regionStart;
  int ni = 0;

  for (int n = 0; n < MAX_NOTES; n++) {
    if (!ch.notes[n].seen)
      continue;

    int size = base + (ni < remainder ? 1 : 0);
    ch.notes[n].regionStart = cursor;
    ch.notes[n].regionSize = size;
    cursor += size;
    ni++;
  }
}

// ============================================================================
// Public API
// ============================================================================

void layout_init() {
  memset(channels, 0, sizeof(channels));
  activeChannelCount = 0;
}

void registerChannel(int channel) {
  if (channel < 0 || channel >= MAX_CHANNELS) {
    return;
  }

  if (!channels[channel].seen) {
    channels[channel].seen = true;
    channels[channel].color = CHANNEL_COLORS[channel];
    channels[channel].seenNoteCount = 0;
    activeChannelCount++;
    recomputeLayout();
  }
}

void registerNote(int channel, int note) {
  if (channel < 0 || channel >= MAX_CHANNELS) {
    return;
  }
  if (note < 0 || note >= MAX_NOTES) {
    return;
  }

  ChannelEntry &ch = channels[channel];

  if (!ch.notes[note].seen) {
    ch.notes[note].seen = true;
    ch.notes[note].active = false;
    ch.seenNoteCount++;
    recomputeLayout();
  }
}

void setNoteActive(int channel, int note, bool active) {
  if (channel < 0 || channel >= MAX_CHANNELS) {
    return;
  }
  if (note < 0 || note >= MAX_NOTES) {
    return;
  }

  channels[channel].notes[note].active = active;
}

void recomputeLayout() {
  if (activeChannelCount == 0) {
    return;
  }

  int totalLEDs = LED_COUNT;
  int chCount = activeChannelCount;
  int base = totalLEDs / chCount;
  int remainder = totalLEDs % chCount;
  int cursor = 0;
  int ci = 0;

  for (int c = 0; c < MAX_CHANNELS; c++) {
    if (!channels[c].seen)
      continue;

    int size = base + (ci < remainder ? 1 : 0);
    channels[c].regionStart = cursor;
    channels[c].regionSize = size;
    cursor += size;

    recomputeNoteLayout(c);
    ci++;
  }
}
