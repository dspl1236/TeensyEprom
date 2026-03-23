// =============================================================================
//  main_lite.cpp — TeensyEprom Lite Firmware
//
//  Pure EPROM emulator + map switcher for Teensy 4.1.
//  Replaces 27C256 / 27C512 in Hitachi MMS ECUs (7A 20v, AAH V6, etc).
//
//  Hardware:
//    2x 74HCT245 @ 3.3V  — address bus level shift (ECU 5V -> 3.3V)
//    Data bus direct       — Teensy 3.3V -> ECU TTL (no buffer needed)
//    /OE, /CE via 1k      — 5V -> 3.3V clamp
//    SD card (built-in)    — ROM storage
//    Button (pin 31)       — map cycling
//    LED (pin 13)          — status
//
//  No wideband, no injector intercept, no CAN, no corrections.
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include "config_lite.h"

// -- ROM buffer ---------------------------------------------------------------
static volatile uint8_t g_rom[ROM_SIZE];
static volatile bool    g_emulating = false;

// -- Map switcher state -------------------------------------------------------
#define MAX_MAPS 16
static char     g_mapFiles[MAX_MAPS][64];
static uint8_t  g_mapCount  = 0;
static uint8_t  g_activeMap = 0;

// -- Fast GPIO ----------------------------------------------------------------

FASTRUN static inline uint16_t read_address() {
    uint16_t addr = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if (digitalReadFast(ADDR_PINS[i])) addr |= (1u << i);
    }
    return addr;
}

FASTRUN static inline void write_data(uint8_t val) {
    for (uint8_t i = 0; i < 8; i++) {
        digitalWriteFast(DATA_PINS[i], (val >> i) & 1);
    }
}

static inline void data_bus_output() {
    for (uint8_t i = 0; i < 8; i++) pinMode(DATA_PINS[i], OUTPUT);
}

static inline void data_bus_hiZ() {
    for (uint8_t i = 0; i < 8; i++) pinMode(DATA_PINS[i], INPUT);
}

// -- EPROM emulation ISR ------------------------------------------------------
//
// /OE falling edge -> read address -> lookup -> drive data -> hold -> release.
// Timing: HD6303 bus cycle ~500ns. ISR total ~250ns. Plenty of margin.

FASTRUN void oe_isr() {
    if (!g_emulating) return;
    if (digitalReadFast(PIN_CE)) return;   // /CE must be LOW

    uint16_t addr = read_address();
    uint8_t  val  = g_rom[addr & 0xFFFF];

    data_bus_output();
    write_data(val);

    while (!digitalReadFast(PIN_OE)) { /* hold until /OE released */ }

    data_bus_hiZ();
}

// -- SD card ------------------------------------------------------------------

static bool load_rom(const char* filename) {
    File f = SD.open(filename, FILE_READ);
    if (!f) {
        Serial.print("ERR: cannot open ");
        Serial.println(filename);
        return false;
    }

    size_t sz = f.size();
    if (sz < ROM_ACTIVE_SIZE) {
        Serial.print("ERR: too small (");
        Serial.print(sz);
        Serial.println("B)");
        f.close();
        return false;
    }

    g_emulating = false;
    noInterrupts();

    f.read((uint8_t*)g_rom, ROM_ACTIVE_SIZE);

    if (sz >= ROM_SIZE) {
        f.read((uint8_t*)g_rom + ROM_ACTIVE_SIZE, ROM_ACTIVE_SIZE);
    } else {
        // Mirror 32KB into upper half (A15-agnostic)
        memcpy((uint8_t*)g_rom + ROM_ACTIVE_SIZE,
               (uint8_t*)g_rom, ROM_ACTIVE_SIZE);
    }

    interrupts();
    g_emulating = true;
    f.close();

    Serial.print("OK: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(sz);
    Serial.println("B)");
    return true;
}

static void scan_maps() {
    g_mapCount = 0;

    File dir = SD.open(MAP_DIR);
    if (dir && dir.isDirectory()) {
        File entry;
        while ((entry = dir.openNextFile()) && g_mapCount < MAX_MAPS) {
            if (!entry.isDirectory()) {
                const char* name = entry.name();
                size_t len = strlen(name);
                if (len > 4 && strcasecmp(name + len - 4, ".bin") == 0) {
                    snprintf(g_mapFiles[g_mapCount], sizeof(g_mapFiles[0]),
                             "%s%s", MAP_DIR, name);
                    g_mapCount++;
                }
            }
            entry.close();
        }
        dir.close();
    }

    if (g_mapCount == 0) {
        if (SD.exists(ROM_FILENAME))
            strncpy(g_mapFiles[g_mapCount++], ROM_FILENAME, 63);
        if (SD.exists(ROM_FALLBACK_FILENAME))
            strncpy(g_mapFiles[g_mapCount++], ROM_FALLBACK_FILENAME, 63);
    }

    Serial.print("Maps: ");
    Serial.println(g_mapCount);
    for (uint8_t i = 0; i < g_mapCount; i++) {
        Serial.print("  ["); Serial.print(i); Serial.print("] ");
        Serial.println(g_mapFiles[i]);
    }
}

// -- LED ----------------------------------------------------------------------

static void led_blink(uint8_t count, uint16_t on_ms = LED_BLINK_MS) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWriteFast(PIN_LED, HIGH); delay(on_ms);
        digitalWriteFast(PIN_LED, LOW);  delay(on_ms);
    }
}

static void led_error() {
    while (true) {
        digitalWriteFast(PIN_LED, HIGH); delay(50);
        digitalWriteFast(PIN_LED, LOW);  delay(50);
    }
}

// -- Button -------------------------------------------------------------------

static void button_check() {
    static uint32_t press_start = 0;
    static bool     was_pressed = false;

    bool pressed = !digitalReadFast(PIN_BUTTON);

    if (pressed && !was_pressed) {
        press_start = millis();
        was_pressed = true;
    }

    if (!pressed && was_pressed) {
        uint32_t held = millis() - press_start;
        was_pressed = false;

        if (g_mapCount == 0) return;

        if (held >= BUTTON_HOLD_MS) {
            g_activeMap = (g_activeMap == 0) ? g_mapCount - 1 : g_activeMap - 1;
        } else if (held >= DEBOUNCE_MS) {
            g_activeMap = (g_activeMap + 1) % g_mapCount;
        } else {
            return;
        }

        Serial.print("-> ["); Serial.print(g_activeMap); Serial.print("] ");
        Serial.println(g_mapFiles[g_activeMap]);
        load_rom(g_mapFiles[g_activeMap]);
        led_blink(g_activeMap + 1);
    }
}

// -- USB serial ---------------------------------------------------------------

static void usb_command(const String& cmd) {
    if (cmd == "LIST") {
        for (uint8_t i = 0; i < g_mapCount; i++) {
            Serial.print(i == g_activeMap ? "* " : "  ");
            Serial.print("["); Serial.print(i); Serial.print("] ");
            Serial.println(g_mapFiles[i]);
        }
    } else if (cmd.startsWith("MAP ")) {
        int idx = cmd.substring(4).toInt();
        if (idx >= 0 && idx < g_mapCount) {
            g_activeMap = idx;
            load_rom(g_mapFiles[g_activeMap]);
            led_blink(g_activeMap + 1);
        } else {
            Serial.println("ERR: bad index");
        }
    } else if (cmd == "DUMP") {
        for (int i = 0; i < 16; i++) {
            if (g_rom[i] < 0x10) Serial.print("0");
            Serial.print(g_rom[i], HEX); Serial.print(" ");
        }
        Serial.println();
    } else if (cmd == "INFO") {
        Serial.print(IDENT_STRING);
        Serial.print("Active: ["); Serial.print(g_activeMap); Serial.print("] ");
        Serial.println(g_mapFiles[g_activeMap]);
        Serial.print("Maps: "); Serial.println(g_mapCount);
        Serial.print("Emulating: "); Serial.println(g_emulating ? "YES" : "NO");
    } else {
        Serial.println("Commands: LIST, MAP <n>, DUMP, INFO");
    }
}

static void usb_read() {
    static String buf;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf.trim();
            if (buf.length()) usb_command(buf);
            buf = "";
        } else {
            buf += c;
        }
    }
}

// -- setup / loop -------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.print(IDENT_STRING);

    pinMode(PIN_LED, OUTPUT);
    digitalWriteFast(PIN_LED, LOW);

    for (uint8_t i = 0; i < 16; i++) pinMode(ADDR_PINS[i], INPUT);
    data_bus_hiZ();
    pinMode(PIN_OE, INPUT);
    pinMode(PIN_CE, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Serial.print("SD: ");
    if (!SD.begin(BUILTIN_SDCARD)) { Serial.println("FAIL"); led_error(); }
    Serial.println("OK");

    scan_maps();
    if (g_mapCount == 0) {
        Serial.println("ERR: no .bin files — put ROMs in /maps/ or root");
        led_error();
    }

    if (!load_rom(g_mapFiles[0])) led_error();

    attachInterrupt(digitalPinToInterrupt(PIN_OE), oe_isr, FALLING);
    g_emulating = true;

    Serial.println("EPROM active");
    led_blink(3, 300);
}

void loop() {
    button_check();
    usb_read();
}
