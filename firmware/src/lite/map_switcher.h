#pragma once
// =============================================================================
//  map_switcher.h  —  Teensy Lite map switching
//  Handles: SD map loading, button ISR, LED feedback, maps.cfg persistence
// =============================================================================

#include <Arduino.h>
#include <SD.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
#define PIN_BUTTON      2       // Momentary switch, active LOW, pulled up
#define PIN_LED         13      // Onboard Teensy LED
#define PIN_SD_CS       BUILTIN_SDCARD

// ── Tunable constants ────────────────────────────────────────────────────────
#define MAX_MAPS        8       // map1.bin … map8.bin on SD
#define ROM_SIZE        32768   // 27C256 = 32KB
#define BTN_DEBOUNCE_MS 50
#define BTN_HOLD_MS     3000    // hold 3s to go back one map
#define BLINK_ON_MS     120
#define BLINK_OFF_MS    100
#define BLINK_GAP_MS    600     // pause between blink groups

// ── Firmware ident (read by PC app on USB connect) ───────────────────────────
#define IDENT_STRING    "IDENT:TEENSY_LITE:v1.0.0\n"

// ── Globals ───────────────────────────────────────────────────────────────────
extern volatile uint8_t  g_rom[ROM_SIZE];   // EPROM emulation buffer (main.cpp)

static uint8_t  s_map_count   = 0;
static uint8_t  s_active_map  = 0;   // 0-based index
static bool     s_sd_ok        = false;
static bool     s_emulating    = false;

// ── LED helpers ───────────────────────────────────────────────────────────────

static void led_blink(uint8_t count, bool fast = false) {
    uint16_t on_ms  = fast ? 60  : BLINK_ON_MS;
    uint16_t off_ms = fast ? 60  : BLINK_OFF_MS;
    digitalWrite(PIN_LED, LOW);
    delay(BLINK_GAP_MS / 2);
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(PIN_LED, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED, LOW);
        if (i < count - 1) delay(off_ms);
    }
    delay(BLINK_GAP_MS / 2);
}

static void led_solid(bool on) {
    digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// Startup ident blink: 3 fast blinks = Lite firmware
static void led_startup_ident() {
    led_blink(3, true);
    delay(300);
    led_blink(3, true);
}

// ── Config file (maps.cfg on SD) ──────────────────────────────────────────────

static void cfg_save() {
    if (!s_sd_ok) return;
    if (SD.exists("/maps.cfg")) SD.remove("/maps.cfg");
    File f = SD.open("/maps.cfg", FILE_WRITE);
    if (f) {
        f.print("active=");
        f.println(s_active_map + 1);   // store 1-based for human readability
        f.print("count=");
        f.println(s_map_count);
        f.close();
    }
}

static void cfg_load() {
    if (!s_sd_ok) return;
    if (!SD.exists("/maps.cfg")) return;
    File f = SD.open("/maps.cfg");
    if (!f) return;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("active=")) {
            int v = line.substring(7).toInt();
            if (v >= 1 && v <= MAX_MAPS) s_active_map = (uint8_t)(v - 1);
        }
    }
    f.close();
}

// ── SD scan: count how many mapN.bin files exist ──────────────────────────────

static uint8_t sd_count_maps() {
    uint8_t n = 0;
    for (uint8_t i = 1; i <= MAX_MAPS; i++) {
        char path[16];
        snprintf(path, sizeof(path), "/map%d.bin", i);
        if (SD.exists(path)) n = i;   // highest found index
        else break;
    }
    return n;
}

// ── Load a map from SD into g_rom buffer ──────────────────────────────────────

static bool map_load(uint8_t index) {
    char path[16];
    snprintf(path, sizeof(path), "/map%d.bin", index + 1);
    if (!SD.exists(path)) return false;
    File f = SD.open(path);
    if (!f) return false;

    // Pause EPROM emulation during load
    s_emulating = false;
    delayMicroseconds(50);   // let any in-flight ISR finish

    uint32_t n = f.read((void*)g_rom, ROM_SIZE);
    f.close();

    if (n < ROM_SIZE) {
        // Pad remainder with 0xFF (blank EPROM)
        memset((void*)(g_rom + n), 0xFF, ROM_SIZE - n);
    }

    s_active_map = index;
    s_emulating  = true;

    // Serial log for PC app console
    Serial.print("MAP_LOADED:");
    Serial.print(index + 1);
    Serial.print(":");
    Serial.println(path);

    return true;
}

// ── Map switching ─────────────────────────────────────────────────────────────

static void map_switch_next() {
    if (s_map_count == 0) return;
    uint8_t next = (s_active_map + 1) % s_map_count;
    led_solid(false);
    if (map_load(next)) {
        cfg_save();
        led_blink(next + 1);   // blink count = map number (1-based)
        led_solid(true);
    }
}

static void map_switch_prev() {
    if (s_map_count == 0) return;
    uint8_t prev = (s_active_map == 0) ? s_map_count - 1 : s_active_map - 1;
    led_solid(false);
    if (map_load(prev)) {
        cfg_save();
        led_blink(prev + 1);
        led_solid(true);
    }
}

// ── Button handler (call from loop(), not ISR — debounce in software) ─────────

static void button_check() {
    static bool     s_last_state  = HIGH;
    static uint32_t s_press_start = 0;
    static bool     s_held_fired  = false;

    bool state = digitalRead(PIN_BUTTON);

    if (state == LOW && s_last_state == HIGH) {
        // Press start
        s_press_start = millis();
        s_held_fired  = false;
    }

    if (state == LOW && !s_held_fired) {
        uint32_t held = millis() - s_press_start;
        if (held >= BTN_HOLD_MS) {
            // Long hold → go back
            s_held_fired = true;
            map_switch_prev();
        }
    }

    if (state == HIGH && s_last_state == LOW) {
        // Release
        uint32_t held = millis() - s_press_start;
        if (held >= BTN_DEBOUNCE_MS && !s_held_fired) {
            // Short press → go forward
            map_switch_next();
        }
    }

    s_last_state = state;
}

// ── USB command handler (subset of full firmware protocol) ───────────────────
//  Commands:  STATUS\n  LIST_MAPS\n  LOAD_MAP:N\n

static void usb_command(const String& cmd) {
    if (cmd == "STATUS") {
        Serial.print("STATUS:LITE:");
        Serial.print(s_active_map + 1);
        Serial.print(":");
        Serial.print(s_map_count);
        Serial.print(":");
        Serial.println(s_sd_ok ? "SD_OK" : "SD_FAIL");

    } else if (cmd == "LIST_MAPS") {
        for (uint8_t i = 0; i < s_map_count; i++) {
            char path[16];
            snprintf(path, sizeof(path), "/map%d.bin", i + 1);
            Serial.print("MAP:");
            Serial.print(i + 1);
            Serial.print(":");
            Serial.print(path);
            Serial.println(i == s_active_map ? ":ACTIVE" : "");
        }
        Serial.println("LIST_END");

    } else if (cmd.startsWith("LOAD_MAP:")) {
        int n = cmd.substring(9).toInt();
        if (n >= 1 && n <= (int)s_map_count) {
            led_solid(false);
            if (map_load(n - 1)) {
                cfg_save();
                led_blink(n);
                led_solid(true);
            }
        } else {
            Serial.println("ERR:INVALID_MAP");
        }
    }
}

// ── Initialise everything ─────────────────────────────────────────────────────

static void map_switcher_init() {
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    led_solid(false);

    // Ident blink: 3 fast pairs = Lite firmware
    led_startup_ident();

    // SD init
    s_sd_ok = SD.begin(PIN_SD_CS);
    if (!s_sd_ok) {
        // Rapid blink error: 10 fast blinks
        led_blink(10, true);
        Serial.println("ERR:SD_FAIL");
        return;
    }

    s_map_count = sd_count_maps();
    if (s_map_count == 0) {
        Serial.println("ERR:NO_MAPS");
        led_blink(5, true);
        return;
    }

    // Restore last active map from config
    cfg_load();
    if (s_active_map >= s_map_count) s_active_map = 0;

    // Load active map into buffer
    if (map_load(s_active_map)) {
        led_solid(true);
        delay(200);
        led_blink(s_active_map + 1);   // show which map is active on startup
        led_solid(true);
    }
}
