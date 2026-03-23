// =============================================================================
// wideband.cpp — Spartan 3 Lite OEM UART parser
//
// Protocol: Send ASCII 'G' to trigger stream.
// Response (each line terminated with LF):
//   0:a:14.70    ← AFR
//   1:a:780      ← LSU sensor temp °C
//   2:a:3000     ← undisclosed
//   3:a:129      ← undisclosed
//
// Serial: WIDEBAND_SERIAL (Serial1), 9600 8N1
// Level shift: Spartan TX (5V) → 10kΩ → junction → Teensy RX
//                                20kΩ junction to GND  (gives 3.3V at Teensy)
// =============================================================================
#include "wideband.h"
#include "config.h"

static float   lastAFR       = 0.0f;
static int     lastTemp_C    = 0;
static uint32_t lastValidMs  = 0;
static bool    valid         = false;

// Line buffer for incoming UART data
static char    lineBuf[32];
static uint8_t linePos = 0;

// ---------------------------------------------------------------------------
void wideband_init() {
  WIDEBAND_SERIAL.begin(WIDEBAND_BAUD);

  // Send 'G' to start streaming
  // Spartan 3 streams continuously after first 'G'
  delay(500);   // Let Spartan boot
  WIDEBAND_SERIAL.print('G');

  Serial.println(F("  Spartan 3: streaming started"));
}

// ---------------------------------------------------------------------------
// parseLine() — parse one "\n"-terminated line from Spartan
// Format: "channel:type:value"
// ---------------------------------------------------------------------------
static void parseLine(const char* line) {
  int channel = line[0] - '0';
  // Skip "X:a:" prefix (4 chars)
  const char* valStr = line + 4;

  switch (channel) {
    case 0:   // AFR
      {
        float afr = atof(valStr);
        if (afr >= AFR_MIN && afr <= AFR_MAX) {
          lastAFR     = afr;
          lastValidMs = millis();
          valid       = true;
        }
      }
      break;
    case 1:   // LSU temp °C
      lastTemp_C = atoi(valStr);
      break;
    default:
      break;  // channels 2,3 undisclosed — ignore for now
  }
}

// ---------------------------------------------------------------------------
void wideband_update() {
  while (WIDEBAND_SERIAL.available()) {
    char c = WIDEBAND_SERIAL.read();
    if (c == '\n') {
      lineBuf[linePos] = '\0';
      if (linePos >= 4) {     // Minimum valid line length
        parseLine(lineBuf);
      }
      linePos = 0;
    } else if (c != '\r' && linePos < (sizeof(lineBuf) - 1)) {
      lineBuf[linePos++] = c;
    }
  }

  // Invalidate if no data for 2 seconds (sensor disconnected/heater cold)
  if (millis() - lastValidMs > 2000) {
    valid = false;
  }
}

// ---------------------------------------------------------------------------
float wideband_getAFR()     { return lastAFR; }
int   wideband_getTemp_C()  { return lastTemp_C; }
bool  wideband_isValid()    { return valid; }
