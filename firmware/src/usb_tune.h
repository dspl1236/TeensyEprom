/**
 * usb_tune.h / usb_tune.cpp
 * Teensy 4.1 USB Serial handler for the Audi90 Tuner app
 *
 * Protocol matches serial_comm/protocol.py exactly.
 *
 * USAGE in main.cpp:
 *   #include "usb_tune.h"
 *   // in setup(): usb_tune_init();
 *   // in loop():  usb_tune_loop();
 *
 * Requires:
 *   - extern uint8_t romData[65536];       // from eprom_emu.h
 *   - extern volatile float g_afr;         // from wideband.cpp
 *   - extern volatile int   g_rpm;         // from sensors.cpp
 *   - extern volatile float g_map_kpa;
 *   - extern volatile float g_tps_pct;
 *   - extern volatile float g_iat_c;
 *   - extern volatile float g_fuel_trim;   // from corrections.cpp
 *   - extern volatile float g_knock_v;     // from sensors.cpp
 *   - extern volatile float g_knock_retard;// from corrections.cpp
 *   - extern bool           g_corrections_enabled; // from corrections.cpp
 *   - extern float          g_target_afr;  // from corrections.cpp
 *   - extern char           g_rom_file[32];// active ROM filename
 *   - void load_rom_from_sd(const char* filename); // from eprom_emu.h
 *   - void save_tune_to_sd(const char* filename);  // from eprom_emu.h
 */

#pragma once
#include <Arduino.h>

// ── Stream interval ───────────────────────────────────────────────────────
#define USB_STREAM_HZ      10     // Live data frames per second
#define USB_STREAM_INTERVAL_MS  (1000 / USB_STREAM_HZ)

// ── Forward declarations (must match your other modules) ──────────────────
extern uint8_t  romData[65536];
extern volatile float  g_afr;
extern volatile int    g_rpm;
extern volatile float  g_map_kpa;
extern volatile float  g_tps_pct;
extern volatile float  g_iat_c;
extern volatile float  g_fuel_trim;
extern volatile float  g_knock_v;
extern volatile float  g_knock_retard;
extern bool            g_corrections_enabled;
extern float           g_target_afr;
extern char            g_rom_file[32];

void load_rom_from_sd(const char* filename);
void save_tune_to_sd(const char* filename);

// ── ROM address constants (893906266D confirmed) ──────────────────────────
#define FUEL_MAP_ADDR    0x0000
#define TIMING_MAP_ADDR  0x1000
#define MAP_SIZE         256   // 16×16 = 256 bytes

// ── SD directory listing helper (implement in your sd/file module) ─────────
// Returns comma-separated list of .bin files on SD root into buf.
extern void list_sd_bin_files(char* buf, size_t buflen);

// ═══════════════════════════════════════════════════════════════════════════
//  usb_tune.cpp  (put in a .cpp file or inline here with #ifdef guard)
// ═══════════════════════════════════════════════════════════════════════════

#ifdef USB_TUNE_IMPL

static uint32_t _last_stream_ms = 0;
static char     _cmd_buf[128];
static uint8_t  _cmd_len = 0;

// ── Init ──────────────────────────────────────────────────────────────────
void usb_tune_init() {
    // USB Serial is always available on Teensy 4.1
    // Nothing to do — Serial is the USB CDC port
    Serial.begin(115200);  // baud ignored on USB CDC, but needed for API
    // Send hello
    delay(500);
    Serial.printf("$STATUS,ready,%lu,%s\n", millis(), g_rom_file);
}

// ── Main loop call ────────────────────────────────────────────────────────
void usb_tune_loop() {
    // 1. Stream live data at USB_STREAM_HZ
    uint32_t now = millis();
    if (now - _last_stream_ms >= USB_STREAM_INTERVAL_MS) {
        _last_stream_ms = now;
        _stream_data();
    }

    // 2. Process incoming commands (non-blocking)
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (_cmd_len > 0) {
                _cmd_buf[_cmd_len] = '\0';
                _process_command(_cmd_buf);
                _cmd_len = 0;
            }
        } else if (_cmd_len < sizeof(_cmd_buf) - 1) {
            _cmd_buf[_cmd_len++] = c;
        }
    }
}

// ── Live data stream ──────────────────────────────────────────────────────
static void _stream_data() {
    // $DATA,rpm,map_kpa,tps_pct,iat_c,afr,fuel_trim_pct,knock_v,knock_retard
    Serial.printf("$DATA,%d,%.1f,%.1f,%.1f,%.2f,%+.1f,%.3f,%.1f\n",
        g_rpm,
        g_map_kpa,
        g_tps_pct,
        g_iat_c,
        g_afr,
        g_fuel_trim,
        g_knock_v,
        g_knock_retard
    );
}

// ── Command dispatcher ────────────────────────────────────────────────────
static void _process_command(const char* cmd) {

    // CMD:PING
    if (strcmp(cmd, "CMD:PING") == 0) {
        Serial.println("ACK:PING");
        return;
    }

    // CMD:GET_FUEL_MAP
    if (strcmp(cmd, "CMD:GET_FUEL_MAP") == 0) {
        Serial.print("MAP:FUEL,");
        for (int i = 0; i < MAP_SIZE; i++) {
            Serial.print(romData[FUEL_MAP_ADDR + i]);
            if (i < MAP_SIZE - 1) Serial.print(",");
        }
        Serial.println();
        return;
    }

    // CMD:GET_TIMING_MAP
    if (strcmp(cmd, "CMD:GET_TIMING_MAP") == 0) {
        Serial.print("MAP:TIMING,");
        for (int i = 0; i < MAP_SIZE; i++) {
            Serial.print(romData[TIMING_MAP_ADDR + i]);
            if (i < MAP_SIZE - 1) Serial.print(",");
        }
        Serial.println();
        return;
    }

    // CMD:SET_CELL,fuel|timing,row,col,value
    if (strncmp(cmd, "CMD:SET_CELL,", 13) == 0) {
        char type[8];
        int row, col, val;
        if (sscanf(cmd + 13, "%7[^,],%d,%d,%d", type, &row, &col, &val) == 4) {
            if (row >= 0 && row < 16 && col >= 0 && col < 16 && val >= 0 && val <= 255) {
                uint16_t base = (strcmp(type, "timing") == 0) ? TIMING_MAP_ADDR : FUEL_MAP_ADDR;
                romData[base + row * 16 + col] = (uint8_t)val;
                Serial.println("ACK:SET_CELL");
                return;
            }
        }
        Serial.println("ERR:SET_CELL_PARSE");
        return;
    }

    // CMD:LOAD_ROM,filename
    if (strncmp(cmd, "CMD:LOAD_ROM,", 13) == 0) {
        const char* fname = cmd + 13;
        load_rom_from_sd(fname);
        Serial.printf("ACK:LOAD_ROM,%s\n", fname);
        Serial.printf("$STATUS,ready,%lu,%s\n", millis(), fname);
        return;
    }

    // CMD:LIST_ROMS
    if (strcmp(cmd, "CMD:LIST_ROMS") == 0) {
        char buf[512] = {0};
        list_sd_bin_files(buf, sizeof(buf));
        Serial.printf("ROMS:%s\n", buf);
        return;
    }

    // CMD:SAVE_MAP
    if (strcmp(cmd, "CMD:SAVE_MAP") == 0) {
        save_tune_to_sd(g_rom_file);
        Serial.println("ACK:SAVE_MAP");
        return;
    }

    // CMD:CORRECTIONS_ON
    if (strcmp(cmd, "CMD:CORRECTIONS_ON") == 0) {
        g_corrections_enabled = true;
        Serial.println("ACK:CORRECTIONS_ON");
        return;
    }

    // CMD:CORRECTIONS_OFF
    if (strcmp(cmd, "CMD:CORRECTIONS_OFF") == 0) {
        g_corrections_enabled = false;
        Serial.println("ACK:CORRECTIONS_OFF");
        return;
    }

    // CMD:SET_TARGET_AFR,value
    if (strncmp(cmd, "CMD:SET_TARGET_AFR,", 19) == 0) {
        float val = atof(cmd + 19);
        if (val >= 10.0f && val <= 18.0f) {
            g_target_afr = val;
            Serial.printf("ACK:SET_TARGET_AFR,%.2f\n", val);
        } else {
            Serial.println("ERR:AFR_RANGE");
        }
        return;
    }

    // Unknown
    Serial.printf("ERR:UNKNOWN_CMD,%s\n", cmd);
}

#endif // USB_TUNE_IMPL
