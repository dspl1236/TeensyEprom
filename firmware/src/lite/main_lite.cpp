// =============================================================================
//  main_lite.cpp  —  Teensy Lite Firmware  (EPROM emulation + map switching)
//
//  Wiring (identical to full firmware EPROM section):
//    A0–A14  → GPIO input  (address bus from ECU)
//    D0–D7   → GPIO output (data bus to ECU)
//    /OE     → GPIO input  (pin 30, active LOW = ECU reading)
//    /CE     → GPIO input  (pin 31, active LOW = chip selected)
//    Button  → pin 2 (momentary, active LOW, pull-up)
//    LED     → pin 13 (onboard Teensy LED)
//    SD card → built-in SD slot on Teensy 4.1
//
//  No wideband, no injector intercept, no CAN, no corrections.
// =============================================================================

#include <Arduino.h>
#include "map_switcher.h"

// ── EPROM emulation buffer ────────────────────────────────────────────────────
volatile uint8_t g_rom[ROM_SIZE];

// ── Address / data bus pin tables (match full firmware layout) ─────────────
static const uint8_t ADDR_PINS[15] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18
};
static const uint8_t DATA_PINS[8] = {
    19, 20, 21, 22, 23, 24, 25, 26
};
static const uint8_t PIN_OE = 30;
static const uint8_t PIN_CE = 31;

// ── Fast GPIO read helpers (Teensy 4.1 direct register) ──────────────────────
FASTRUN static inline uint16_t read_address() {
    uint16_t addr = 0;
    for (uint8_t i = 0; i < 15; i++) {
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

static inline void data_bus_input() {
    for (uint8_t i = 0; i < 8; i++) pinMode(DATA_PINS[i], INPUT);
}

// ── EPROM emulation ISR ───────────────────────────────────────────────────────
// Triggered on falling edge of /OE (ECU initiating a read cycle)

FASTRUN void oe_isr() {
    if (!s_emulating) return;
    if (digitalReadFast(PIN_CE)) return;   // /CE must also be low

    uint16_t addr = read_address() & 0x7FFF;   // 27C256 = 15-bit address
    uint8_t  val  = g_rom[addr];

    data_bus_output();
    write_data(val);

    // Hold data while /OE is active, then release bus
    while (!digitalReadFast(PIN_OE)) { /* spin ≈ 150ns max */ }
    data_bus_input();
}

// ── USB serial command reader ─────────────────────────────────────────────────
static void usb_read() {
    static String s_buf;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            s_buf.trim();
            if (s_buf.length()) usb_command(s_buf);
            s_buf = "";
        } else {
            s_buf += c;
        }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);   // give USB host time to enumerate

    // Address bus as inputs
    for (uint8_t i = 0; i < 15; i++) pinMode(ADDR_PINS[i], INPUT);
    // Data bus starts as input (tri-stated)
    data_bus_input();
    // /OE and /CE as inputs
    pinMode(PIN_OE, INPUT);
    pinMode(PIN_CE, INPUT);

    // ISR on /OE falling edge
    attachInterrupt(digitalPinToInterrupt(PIN_OE), oe_isr, FALLING);

    // Map switcher init (SD, LED, button, load active map)
    map_switcher_init();

    // Send ident string so PC app knows which firmware is running
    Serial.print(IDENT_STRING);
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    button_check();   // debounced button handling (fwd/hold-back)
    usb_read();       // USB command from PC app
}
