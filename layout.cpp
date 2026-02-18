#include "layout.h"
#include <cstdio>
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

ChannelEntry channels[MAX_CHANNELS];
int activeChannelCount = 0;

// ============================================================================
// Color Palette
// ============================================================================

static const uint32_t CHANNEL_COLORS[16] = {
    0xFF0000, // ch 0  Red
    0xFF8000, // ch 1  Orange
    0xFFFF00, // ch 2  Yellow
    0x80FF00, // ch 3  Chartreuse
    0x00FF00, // ch 4  Green
    0x00FF80, // ch 5  Spring Green
    0x00FFFF, // ch 6  Cyan
    0x0080FF, // ch 7  Azure
    0x0000FF, // ch 8  Blue
    0x8000FF, // ch 9  Violet
    0xFF00FF, // ch 10 Magenta
    0xFF0080, // ch 11 Rose
    0x8B4513, // ch 12 SaddleBrown
    0x008080, // ch 13 Teal
    0x800080, // ch 14 Purple
    0x708090, // ch 15 SlateGray
};

// ============================================================================
// Recursive Binary Space Partitioning (BSP) for Layout
// ============================================================================

// Helper: Split a parent rect into N sections recursively
void computeTiling(Rect area, int n, Rect **out_rects) {
  if (n <= 0)
    return;

  // Base case: One item gets the whole area
  if (n == 1) {
    *(out_rects[0]) = area;
    return;
  }

  // Split decision: Cut the longer Dimension to keep aspect ratio square-ish
  bool splitVertically = (area.w >= area.h);

  // Split items roughly in half
  int k = n / 2; // Items in first part

  Rect parts[2];

  if (splitVertically) {
    // Split Width
    int w1 = (area.w * k) / n;
    int w2 = area.w - w1;
    parts[0] = {area.x, area.y, w1, area.h};
    parts[1] = {area.x + w1, area.y, w2, area.h};
  } else {
    // Split Height
    int h1 = (area.h * k) / n;
    int h2 = area.h - h1;
    parts[0] = {area.x, area.y, area.w, h1};
    parts[1] = {area.x, area.y + h1, area.w, h2};
  }

  // Recurse Left (k items)
  computeTiling(parts[0], k, out_rects);

  // Recurse Right (n-k items)
  computeTiling(parts[1], n - k, out_rects + k);
}

// Recompute regions for all active channels (2D Tiled)
void recomputeLayout() {
  if (activeChannelCount == 0)
    return;

  // 1. Gather pointers to active channels for the tiler
  Rect *targets[MAX_CHANNELS];
  int t_idx = 0;
  ChannelEntry *activePtrs[MAX_CHANNELS];

  for (int c = 0; c < MAX_CHANNELS; c++) {
    if (channels[c].seen) {
      activePtrs[t_idx] = &channels[c];
      targets[t_idx] = &channels[c].bounds;
      t_idx++;
    }
  }

  // 2. Compute Tiling for Channels
  Rect fullScreen = {0, 0, PANEL_WIDTH, PANEL_HEIGHT};
  if (t_idx > 0) {
    printf("Recomputing Layout 2D: %d items\n", t_idx);
    computeTiling(fullScreen, t_idx, targets);
  }

  // 3. Recompute Note Layouts for each channel
  for (int i = 0; i < t_idx; i++) {
    ChannelEntry *ch = activePtrs[i];

    // Count SEEN notes in this channel (for stable tiling)
    int seenNotes = 0;
    Rect *noteTargets[MAX_NOTES];

    for (int n = 0; n < MAX_NOTES; n++) {
      // Only condition is that the note has been seen
      if (ch->notes[n].seen) {
        noteTargets[seenNotes] = &ch->notes[n].bounds;
        seenNotes++;
      }
    }

    if (seenNotes > 0) {
      printf("  Ch %d: %d seen notes (tiling)\n", i, seenNotes);
      computeTiling(ch->bounds, seenNotes, noteTargets);
    }
  }
}

// ============================================================================
// Public API
// ============================================================================

void layout_init() { layout_reset(); }

void layout_reset() {
  memset(channels, 0, sizeof(channels));
  activeChannelCount = 0;
  printf("Layout Reset!\n");
}

void registerChannel(int channel) {
  if (channel < 0 || channel >= MAX_CHANNELS)
    return;

  if (!channels[channel].seen) {
    channels[channel].seen = true;
    channels[channel].color = CHANNEL_COLORS[channel];
    channels[channel].seenNoteCount = 0;
    activeChannelCount++;
    recomputeLayout();
  }
}

void registerNote(int channel, int note) {
  if (channel < 0 || channel >= MAX_CHANNELS)
    return;
  if (note < 0 || note >= MAX_NOTES)
    return;

  ChannelEntry &ch = channels[channel];

  if (!ch.notes[note].seen) {
    ch.notes[note].seen = true;
    ch.notes[note].active = false;
    ch.seenNoteCount++;
    recomputeLayout();
  }
}

void setNoteActive(int channel, int note, bool active) {
  if (channel < 0 || channel >= MAX_CHANNELS)
    return;
  if (note < 0 || note >= MAX_NOTES)
    return;

  channels[channel].notes[note].active = active;
  // Trigger recompute (though strictly only needed if 'seen' changed, keeping
  // it simple)
  recomputeLayout();
}
