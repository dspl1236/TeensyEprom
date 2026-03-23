// =============================================================================
//  main.cpp — TeensyEprom v2.0 Firmware
//
//  EPROM emulator + map switcher for Teensy 4.1.
//  Replaces 27C256 / 27C512 in Hitachi MMS ECUs (7A 20v, AAH V6, etc).
//
//  Storage: LittleFS on internal flash (primary), SD card (fallback).
//  Upload:  USB serial binary transfer with CRC32 verification.
//
//  No wideband, no injector intercept, no CAN, no corrections.
// =============================================================================

#include <Arduino.h>
#include <LittleFS.h>
#include <SD.h>
#include "config.h"

// -- Filesystem ---------------------------------------------------------------
LittleFS_Program lfs;
static bool g_lfs_ok = false;
static bool g_sd_ok  = false;

// -- ROM buffer ---------------------------------------------------------------
static volatile uint8_t g_rom[ROM_SIZE];
static volatile bool    g_emulating = false;

// -- Map switcher state -------------------------------------------------------
static char     g_mapFiles[MAX_MAPS][64];
static uint8_t  g_mapCount  = 0;
static uint8_t  g_activeMap = 0;

// =============================================================================
// CRC32 — for upload verification
// =============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
    0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,0x09B64C2B,0x7EB17CBE,0xE7B82D09,0x90BF1D9F,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
    0x35B5A8FA,0x42B2986C,0xDBBBB9D6,0xACBCB9C0,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F6B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
    0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0D6B,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
    0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
    0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7A23,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
    0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5ACE,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
    0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA8670591,0x316E547E,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
    0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F6,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6B70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
    0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD706FF,0x54DE5729,0x23D967BF,
    0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

static uint32_t crc32(const uint8_t* buf, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// Fast GPIO — EPROM bus interface
// =============================================================================

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

// =============================================================================
// EPROM emulation ISR
// =============================================================================
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

// =============================================================================
// Storage — LittleFS primary, SD fallback
// =============================================================================

// Open a file from whichever filesystem has it.
// Priority: LittleFS first, SD second.
static File open_file(const char* path, uint8_t mode = FILE_READ) {
    if (g_lfs_ok) {
        File f = lfs.open(path, mode);
        if (f) return f;
    }
    // Don't fall back to SD for writes — LittleFS is primary
    if (mode == FILE_READ && g_sd_ok) {
        return SD.open(path, mode);
    }
    return File();
}

static bool load_rom(const char* filename) {
    File f;

    // Try LittleFS first
    if (g_lfs_ok) f = lfs.open(filename, FILE_READ);
    // Fall back to SD
    if (!f && g_sd_ok) f = SD.open(filename, FILE_READ);

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

    // Pause emulation during load
    g_emulating = false;
    noInterrupts();

    f.read((uint8_t*)g_rom, ROM_ACTIVE_SIZE);

    if (sz >= ROM_SIZE) {
        // 64KB file — read upper half directly
        f.read((uint8_t*)g_rom + ROM_ACTIVE_SIZE, ROM_ACTIVE_SIZE);
    } else {
        // 32KB file — mirror into upper half (A15-agnostic)
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

// ---------------------------------------------------------------------------
// Map directory scanning
// ---------------------------------------------------------------------------

static void scan_maps_lfs() {
    if (!g_lfs_ok) return;

    // Ensure /maps/ exists
    lfs.mkdir(MAP_DIR);

    File dir = lfs.open(MAP_DIR);
    if (!dir || !dir.isDirectory()) return;

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

static void scan_maps_sd() {
    if (!g_sd_ok || g_mapCount >= MAX_MAPS) return;

    File dir = SD.open(SD_MAP_DIR);
    if (dir && dir.isDirectory()) {
        File entry;
        while ((entry = dir.openNextFile()) && g_mapCount < MAX_MAPS) {
            if (!entry.isDirectory()) {
                const char* name = entry.name();
                size_t len = strlen(name);
                if (len > 4 && strcasecmp(name + len - 4, ".bin") == 0) {
                    snprintf(g_mapFiles[g_mapCount], sizeof(g_mapFiles[0]),
                             "%s%s", SD_MAP_DIR, name);
                    g_mapCount++;
                }
            }
            entry.close();
        }
        dir.close();
    }

    // Also check root for tune.bin / stock.bin
    if (g_mapCount == 0) {
        if (SD.exists(SD_FALLBACK_FILE)) {
            strncpy(g_mapFiles[g_mapCount++], SD_FALLBACK_FILE, 63);
        }
        if (SD.exists("stock.bin")) {
            strncpy(g_mapFiles[g_mapCount++], "stock.bin", 63);
        }
    }
}

static void scan_maps() {
    g_mapCount = 0;

    // LittleFS first (primary)
    scan_maps_lfs();
    uint8_t lfs_count = g_mapCount;

    // SD fallback only if LittleFS had nothing
    if (g_mapCount == 0) {
        scan_maps_sd();
    }

    Serial.print("Maps: ");
    Serial.print(g_mapCount);
    if (lfs_count > 0) Serial.print(" (flash)");
    else if (g_mapCount > 0) Serial.print(" (SD fallback)");
    Serial.println();

    for (uint8_t i = 0; i < g_mapCount; i++) {
        Serial.print("  ["); Serial.print(i); Serial.print("] ");
        Serial.println(g_mapFiles[i]);
    }
}

// ---------------------------------------------------------------------------
// Active map persistence
// ---------------------------------------------------------------------------

static void save_active_index() {
    if (!g_lfs_ok) return;
    File f = lfs.open(ACTIVE_FILE, FILE_WRITE_BEGIN);
    if (f) {
        f.print(g_activeMap);
        f.close();
    }
}

static uint8_t load_active_index() {
    if (!g_lfs_ok) return 0;
    File f = lfs.open(ACTIVE_FILE, FILE_READ);
    if (!f) return 0;
    int idx = f.parseInt();
    f.close();
    if (idx >= 0 && idx < g_mapCount) return (uint8_t)idx;
    return 0;
}

// =============================================================================
// LED
// =============================================================================

static void led_blink(uint8_t count, uint16_t on_ms = LED_BLINK_MS) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWriteFast(PIN_LED, HIGH); delay(on_ms);
        digitalWriteFast(PIN_LED, LOW);  delay(on_ms);
    }
}

static void led_error() {
    // Fast blink forever — something critical failed
    while (true) {
        digitalWriteFast(PIN_LED, HIGH); delay(50);
        digitalWriteFast(PIN_LED, LOW);  delay(50);
    }
}

// =============================================================================
// Button — map cycling
// =============================================================================

static void switch_to_map(uint8_t idx) {
    if (idx >= g_mapCount) return;
    g_activeMap = idx;
    Serial.print("-> ["); Serial.print(g_activeMap); Serial.print("] ");
    Serial.println(g_mapFiles[g_activeMap]);
    load_rom(g_mapFiles[g_activeMap]);
    save_active_index();
    led_blink(g_activeMap + 1);
}

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

        uint8_t next;
        if (held >= BUTTON_HOLD_MS) {
            // Long press = previous map
            next = (g_activeMap == 0) ? g_mapCount - 1 : g_activeMap - 1;
        } else if (held >= DEBOUNCE_MS) {
            // Short press = next map
            next = (g_activeMap + 1) % g_mapCount;
        } else {
            return;  // bounce
        }

        switch_to_map(next);
    }
}

// =============================================================================
// USB serial protocol
// =============================================================================

// ---------------------------------------------------------------------------
// UPLOAD — binary ROM transfer with CRC32 verification
// ---------------------------------------------------------------------------
//   Host: UPLOAD /maps/tune.bin 32768\n
//   Dev:  READY 32768\n
//   Host: <32768 raw bytes><4 bytes CRC32 big-endian>
//   Dev:  OK /maps/tune.bin 32768 CRC32=XXXXXXXX\n
//      or ERR: <reason>\n

static void cmd_upload(const String& args) {
    if (!g_lfs_ok) {
        Serial.println("ERR: flash not available");
        return;
    }

    // Parse: filename size
    int space = args.indexOf(' ');
    if (space < 0) {
        Serial.println("ERR: usage: UPLOAD <filename> <size>");
        return;
    }

    String filename = args.substring(0, space);
    filename.trim();
    uint32_t size = args.substring(space + 1).toInt();

    if (size < ROM_ACTIVE_SIZE || size > ROM_SIZE) {
        Serial.print("ERR: size must be ");
        Serial.print(ROM_ACTIVE_SIZE);
        Serial.print("-");
        Serial.println(ROM_SIZE);
        return;
    }

    // Ensure directory exists
    lfs.mkdir(MAP_DIR);

    // Signal ready
    Serial.print("READY ");
    Serial.println(size);
    Serial.flush();

    // Receive raw bytes into temp buffer
    static uint8_t upload_buf[ROM_SIZE];
    uint32_t received = 0;
    uint32_t start_ms = millis();

    while (received < size) {
        if (millis() - start_ms > UPLOAD_TIMEOUT_MS) {
            Serial.print("ERR: timeout at ");
            Serial.print(received);
            Serial.print("/");
            Serial.println(size);
            return;
        }
        int avail = Serial.available();
        if (avail > 0) {
            int chunk = min((uint32_t)avail, size - received);
            Serial.readBytes(upload_buf + received, chunk);
            received += chunk;
            start_ms = millis();  // reset timeout on progress
        }
    }

    // Receive 4-byte CRC32 (big-endian)
    uint8_t crc_bytes[4];
    uint32_t crc_received = 0;
    start_ms = millis();
    while (crc_received < 4) {
        if (millis() - start_ms > UPLOAD_TIMEOUT_MS) {
            Serial.println("ERR: timeout waiting for CRC");
            return;
        }
        if (Serial.available()) {
            crc_bytes[crc_received++] = Serial.read();
            start_ms = millis();
        }
    }

    uint32_t expected_crc = ((uint32_t)crc_bytes[0] << 24) |
                            ((uint32_t)crc_bytes[1] << 16) |
                            ((uint32_t)crc_bytes[2] <<  8) |
                            ((uint32_t)crc_bytes[3]);

    uint32_t actual_crc = crc32(upload_buf, size);

    if (actual_crc != expected_crc) {
        Serial.print("ERR: CRC mismatch (expected ");
        Serial.print(expected_crc, HEX);
        Serial.print(" got ");
        Serial.print(actual_crc, HEX);
        Serial.println(")");
        return;
    }

    // Write to LittleFS
    File f = lfs.open(filename.c_str(), FILE_WRITE_BEGIN);
    if (!f) {
        Serial.print("ERR: cannot create ");
        Serial.println(filename);
        return;
    }

    f.write(upload_buf, size);
    f.close();

    Serial.print("OK ");
    Serial.print(filename);
    Serial.print(" ");
    Serial.print(size);
    Serial.print(" CRC32=");
    Serial.println(actual_crc, HEX);

    // Rescan maps
    scan_maps();
}

// ---------------------------------------------------------------------------
// Standard commands
// ---------------------------------------------------------------------------

static void cmd_list() {
    for (uint8_t i = 0; i < g_mapCount; i++) {
        Serial.print(i == g_activeMap ? "* " : "  ");
        Serial.print("["); Serial.print(i); Serial.print("] ");
        Serial.println(g_mapFiles[i]);
    }
    if (g_mapCount == 0) Serial.println("(no maps)");
}

static void cmd_info() {
    Serial.print(IDENT_STRING);
    Serial.print("Storage: ");
    if (g_lfs_ok) Serial.print("flash(OK)");
    else Serial.print("flash(FAIL)");
    Serial.print(" sd(");
    Serial.print(g_sd_ok ? "OK" : "none");
    Serial.println(")");
    Serial.print("Maps: "); Serial.println(g_mapCount);
    if (g_mapCount > 0) {
        Serial.print("Active: ["); Serial.print(g_activeMap); Serial.print("] ");
        Serial.println(g_mapFiles[g_activeMap]);
    }
    Serial.print("Emulating: "); Serial.println(g_emulating ? "YES" : "NO");
}

static void cmd_dump() {
    // Hex dump first 256 bytes (row 0 of fuel map on Hitachi MMS)
    for (int row = 0; row < 16; row++) {
        if (row * 16 < 0x10) Serial.print("0");
        Serial.print(row * 16, HEX);
        Serial.print(": ");
        for (int col = 0; col < 16; col++) {
            uint8_t v = g_rom[row * 16 + col];
            if (v < 0x10) Serial.print("0");
            Serial.print(v, HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

static void cmd_delete(const String& filename) {
    if (!g_lfs_ok) {
        Serial.println("ERR: flash not available");
        return;
    }
    if (lfs.remove(filename.c_str())) {
        Serial.print("OK: deleted ");
        Serial.println(filename);
        scan_maps();
        // If active map was deleted, switch to 0
        if (g_activeMap >= g_mapCount && g_mapCount > 0) {
            switch_to_map(0);
        }
    } else {
        Serial.print("ERR: cannot delete ");
        Serial.println(filename);
    }
}

static void cmd_format() {
    Serial.println("Formatting flash...");
    g_emulating = false;
    lfs.quickFormat();
    lfs.mkdir(MAP_DIR);
    g_mapCount = 0;
    g_activeMap = 0;
    Serial.println("OK: formatted");
}

static void usb_command(const String& cmd) {
    if (cmd == "INFO") {
        cmd_info();
    } else if (cmd == "LIST") {
        cmd_list();
    } else if (cmd.startsWith("MAP ")) {
        int idx = cmd.substring(4).toInt();
        if (idx >= 0 && idx < g_mapCount) {
            switch_to_map(idx);
        } else {
            Serial.println("ERR: bad index");
        }
    } else if (cmd == "DUMP") {
        cmd_dump();
    } else if (cmd.startsWith("UPLOAD ")) {
        cmd_upload(cmd.substring(7));
    } else if (cmd.startsWith("DELETE ")) {
        cmd_delete(cmd.substring(7));
    } else if (cmd == "FORMAT") {
        cmd_format();
    } else if (cmd == "RESCAN") {
        scan_maps();
    } else {
        Serial.println("Commands: INFO, LIST, MAP <n>, DUMP, UPLOAD <file> <size>, DELETE <file>, FORMAT, RESCAN");
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

// =============================================================================
// setup / loop
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.print(IDENT_STRING);

    // GPIO init
    pinMode(PIN_LED, OUTPUT);
    digitalWriteFast(PIN_LED, LOW);
    for (uint8_t i = 0; i < 16; i++) pinMode(ADDR_PINS[i], INPUT);
    data_bus_hiZ();
    pinMode(PIN_OE, INPUT);
    pinMode(PIN_CE, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // LittleFS — primary storage
    Serial.print("Flash: ");
    g_lfs_ok = lfs.begin(LFS_SIZE);
    if (!g_lfs_ok) {
        // First boot or corrupted — format and retry
        Serial.print("format... ");
        lfs.format();
        g_lfs_ok = lfs.begin(LFS_SIZE);
    }
    if (g_lfs_ok) {
        lfs.mkdir(MAP_DIR);
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }

    // SD card — fallback
    Serial.print("SD: ");
    g_sd_ok = SD.begin(BUILTIN_SDCARD);
    Serial.println(g_sd_ok ? "OK" : "none");

    // Scan for maps
    scan_maps();
    if (g_mapCount == 0) {
        Serial.println("No maps found. Upload via USB: UPLOAD /maps/tune.bin 32768");
        Serial.println("Waiting for upload...");
        // Don't led_error — just wait for USB upload
        digitalWriteFast(PIN_LED, HIGH);  // solid LED = waiting
        while (g_mapCount == 0) {
            usb_read();
            delay(10);
        }
        digitalWriteFast(PIN_LED, LOW);
    }

    // Load active map
    g_activeMap = load_active_index();
    if (g_activeMap >= g_mapCount) g_activeMap = 0;

    if (!load_rom(g_mapFiles[g_activeMap])) {
        // Try first available map
        if (g_mapCount > 0) {
            g_activeMap = 0;
            if (!load_rom(g_mapFiles[0])) led_error();
        } else {
            led_error();
        }
    }

    // Attach ISR — start emulating
    attachInterrupt(digitalPinToInterrupt(PIN_OE), oe_isr, FALLING);
    g_emulating = true;

    Serial.println("EPROM active");
    led_blink(3, 300);  // 3 slow blinks = running
}

void loop() {
    button_check();
    usb_read();
}
