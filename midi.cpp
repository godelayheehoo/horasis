#include "midi.h"
#include "config.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"


// MIDI parser state machine
enum MidiState { WAITING_STATUS, WAITING_DATA1, WAITING_DATA2 };

static MidiState state = WAITING_STATUS;
static uint8_t runningStatus = 0;
static uint8_t currentStatus = 0;
static uint8_t data1 = 0;
static bool inSysEx = false;

// ============================================================================
// Helper Functions
// ============================================================================

static inline bool isStatusByte(uint8_t b) { return (b & 0x80) != 0; }

static inline bool isDataByte(uint8_t b) { return (b & 0x80) == 0; }

static inline uint8_t getMessageType(uint8_t status) { return status & 0xF0; }

static inline uint8_t getChannel(uint8_t status) { return status & 0x0F; }

// ============================================================================
// Message Handlers
// ============================================================================

static void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  (void)velocity; // Unused
  onNoteOff(channel, note);
}

static void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  if (velocity == 0) {
    // Note On with velocity 0 is treated as Note Off
    onNoteOff(channel, note);
  } else {
    onNoteOn(channel, note, velocity);
  }
}

static void handleControlChange(uint8_t channel, uint8_t controller,
                                uint8_t value) {
  (void)value; // Unused

  // All Notes Off (CC 123 / 0x7B)
  if (controller == 0x7B) {
    // Fire note-off for all 128 possible notes on this channel
    for (int note = 0; note < 128; note++) {
      onNoteOff(channel, note);
    }
  }
  // All other CCs are ignored
}

// ============================================================================
// State Machine
// ============================================================================

static void processByte(uint8_t b) {
  // Handle SysEx mode
  if (inSysEx) {
    if (b == 0xF7) {
      inSysEx = false; // End of SysEx
    }
    return; // Consume all SysEx bytes
  }

  // Check for new status byte
  if (isStatusByte(b)) {
    if (b == 0xF0) {
      // Start of SysEx
      inSysEx = true;
      runningStatus = 0; // Clear running status
      return;
    }

    if (b >= 0xF8) {
      // Real-time messages (ignore)
      return;
    }

    if (b >= 0xF0) {
      // System Common messages (ignore, clear running status)
      runningStatus = 0;
      return;
    }

    // Channel voice message
    currentStatus = b;
    runningStatus = b;
    state = WAITING_DATA1;
    return;
  }

  // Data byte handling
  if (!isDataByte(b)) {
    return; // Invalid byte, ignore
  }

  switch (state) {
  case WAITING_STATUS: {
    // Unexpected data byte - use running status if available
    if (runningStatus != 0) {
      currentStatus = runningStatus;
      data1 = b;

      uint8_t msgType = getMessageType(currentStatus);
      if (msgType == 0xC0 || msgType == 0xD0) {
        // Program Change and Channel Pressure have only 1 data byte
        state = WAITING_STATUS;
      } else {
        state = WAITING_DATA2;
      }
    }
    break;
  }

  case WAITING_DATA1: {
    data1 = b;

    uint8_t msgType = getMessageType(currentStatus);
    if (msgType == 0xC0 || msgType == 0xD0) {
      // Program Change and Channel Pressure have only 1 data byte
      state = WAITING_STATUS;
    } else {
      state = WAITING_DATA2;
    }
    break;
  }

  case WAITING_DATA2: {
    uint8_t data2 = b;
    uint8_t channel = getChannel(currentStatus);
    uint8_t msgType = getMessageType(currentStatus);

    // Dispatch message
    switch (msgType) {
    case 0x80: // Note Off
      handleNoteOff(channel, data1, data2);
      break;

    case 0x90: // Note On
      handleNoteOn(channel, data1, data2);
      break;

    case 0xB0: // Control Change
      handleControlChange(channel, data1, data2);
      break;

    // All other message types are silently ignored
    default:
      break;
    }

    state = WAITING_STATUS;
    break;
  }
  }
}

// ============================================================================
// Public API
// ============================================================================

void midi_init() {
  // Initialize UART0 at 31,250 baud
  uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);

  // Set TX and RX pins
  gpio_set_function(MIDI_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MIDI_RX_PIN, GPIO_FUNC_UART);

  // Configure UART: 8 data bits, 1 stop bit, no parity
  uart_set_format(MIDI_UART_ID, 8, 1, UART_PARITY_NONE);

  // Disable FIFO (we're polling, not using interrupts)
  uart_set_fifo_enabled(MIDI_UART_ID, false);
}

void midi_poll() {
  // Read all available bytes
  while (uart_is_readable(MIDI_UART_ID)) {
    uint8_t b = uart_getc(MIDI_UART_ID);
    processByte(b);
  }
}
