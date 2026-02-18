#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>

// Initialize MIDI UART (31,250 baud on UART0)
void midi_init();

// Poll for incoming MIDI bytes and fire callbacks
// Call this frequently from the main loop
void midi_poll();

// Callbacks implemented by main.cpp
// These are called when MIDI messages are parsed
extern void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
extern void onNoteOff(uint8_t channel, uint8_t note);

#endif // MIDI_H
