#ifndef ERNI_FRAMING_H
#define ERNI_FRAMING_H

#include <Arduino.h>

// KISS Protocol Constants
#define FEND 0xC0
#define FESC 0xDB
#define TFEND 0xDC
#define TFESC 0xDD
#define KISS_CMD_DATA 0x00

/**
 * Sends a raw buffer up the serial port encapsulated in the KISS protocol.
 * The Reticulum Network Stack (RNS) uses this exact framing to read our MTU
 * packets. Includes the standard KISS command byte (0x00 = Data on port 0).
 */
void send_to_serial_kiss(uint8_t *data, int len) {
  Serial.write(FEND);
  Serial.write(KISS_CMD_DATA);
  for (int i = 0; i < len; i++) {
    if (data[i] == FEND) {
      Serial.write(FESC);
      Serial.write(TFEND);
    } else if (data[i] == FESC) {
      Serial.write(FESC);
      Serial.write(TFESC);
    } else {
      Serial.write(data[i]);
    }
  }
  Serial.write(FEND);
}

#endif // ERNI_FRAMING_H
