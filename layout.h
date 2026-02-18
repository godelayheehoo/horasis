#ifndef LAYOUT_H
#define LAYOUT_H

#include "config.h"
#include <stdint.h>


// ============================================================================
// Data Structures
// ============================================================================

struct NoteEntry {
  bool seen;       // Has this note ever fired on this channel?
  bool active;     // Is a note-on currently held?
  int regionStart; // First LED index in this note's sub-region
  int regionSize;  // Number of LEDs in this note's sub-region
};

struct ChannelEntry {
  bool seen;         // Has this channel been detected?
  uint32_t color;    // GRB color assigned at first detection
  int regionStart;   // First LED index of channel's region
  int regionSize;    // Number of LEDs in channel's region
  int seenNoteCount; // How many distinct notes seen so far
  NoteEntry notes[MAX_NOTES];
};

// ============================================================================
// Global State
// ============================================================================

extern ChannelEntry channels[MAX_CHANNELS];
extern int activeChannelCount;

// ============================================================================
// Public API
// ============================================================================

// Initialize layout engine (zero all state)
void layout_init();

// Register a channel (if not already seen) and assign color
void registerChannel(int channel);

// Register a note on a channel (if not already seen)
void registerNote(int channel, int note);

// Set note active state (does not trigger reflow)
void setNoteActive(int channel, int note, bool active);

// Recompute all region boundaries (called after new channel/note detected)
void recomputeLayout();

#endif // LAYOUT_H
