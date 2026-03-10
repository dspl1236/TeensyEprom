// =============================================================================
// config.h — Pin assignments and compile-time constants
// Teensy 4.1
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// ROM
// ---------------------------------------------------------------------------
#define ROM_SIZE          65536       // 64KB total (27C512)
#define ROM_ACTIVE_SIZE   32768       // 32KB active (mirrored)
#define ROM_FILENAME      "tune.bin"  // Active tune on SD card
#define ROM_FALLBACK_FILENAME "stock.bin" // Stock ROM fallback

// ---------------------------------------------------------------------------
// EPROM emulator — FlexIO address bus (A0–A15)
// 74HCT245 level shifters between ECU (5V) and Teensy (3.3V)
//
// NOTE: Final pin assignments must be verified against FlexIO1/2/3 mux
//       tables in IMXRT1062 reference manual before PCB layout.
//       These are placeholders based on FlexIO2 availability on Teensy 4.1.
// ---------------------------------------------------------------------------

// Address lines — 16 bits (A0–A15)
// Using FlexIO2 shifter/timer for parallel capture
#define PIN_A0    19
#define PIN_A1    18
#define PIN_A2    17
#define PIN_A3    16
#define PIN_A4    15
#define PIN_A5    14
#define PIN_A6    40
#define PIN_A7    41
#define PIN_A8    42
#define PIN_A9    43
#define PIN_A10   44
#define PIN_A11   45
#define PIN_A12   6
#define PIN_A13   9
#define PIN_A14   32
#define PIN_A15   8

// Data lines — 8 bits (D0–D7)
// FlexIO1 shifter for parallel output
#define PIN_D0    2
#define PIN_D1    3
#define PIN_D2    4
#define PIN_D3    5
#define PIN_D4    33
#define PIN_D5    34
#define PIN_D6    35
#define PIN_D7    36

// EPROM control signals (active LOW, from ECU via level shifter)
#define PIN_OE    37    // /OE — Output Enable
#define PIN_CE    38    // /CE — Chip Enable

// ---------------------------------------------------------------------------
// Wideband — Spartan 3 Lite OEM UART
// 9600 baud 8N1, 5V TX → voltage divider → Teensy RX
// ---------------------------------------------------------------------------
#define WIDEBAND_SERIAL   Serial1
#define WIDEBAND_BAUD     9600
#define PIN_WB_TX         1     // Teensy TX1 → Spartan RX (unused, reserve)
#define PIN_WB_RX         0     // Spartan TX → 10k/20k divider → Teensy RX1

// AFR sanity limits
#define AFR_MIN           10.0f
#define AFR_MAX           20.0f
#define AFR_TARGET        14.7f  // Stoichiometric (override per MAP region)

// ---------------------------------------------------------------------------
// Sensors
// ---------------------------------------------------------------------------

// GM 3-bar MAP sensor #55567257
// Supply 5V, output 0.5–4.5V → 10k/20k divider → Teensy ADC (0–3.3V)
#define PIN_MAP           A1
#define MAP_SUPPLY_V      5.0f
#define MAP_V_MIN         0.5f   // 0 kPa absolute
#define MAP_V_MAX         4.5f   // 300 kPa absolute
#define MAP_KPA_MIN       10.0f
#define MAP_KPA_MAX       304.0f

// Bosch knock sensor 0261231006
// AC signal biased to 1.65V (Vcc/2) via 100k+100k divider
// Peak detection in software
#define PIN_KNOCK         A2
#define KNOCK_BIAS_V      1.65f
#define KNOCK_THRESHOLD_V 0.3f   // Above bias = knock event (tune this!)

// TPS — tee off stock sensor signal, voltage divider to 3.3V
#define PIN_TPS           A3
#define TPS_V_MIN         0.5f   // 0% throttle
#define TPS_V_MAX         4.5f   // 100% throttle

// IAT — stock NTC sensor or new sensor tee'd in
#define PIN_IAT           A4

// RPM input — coil trigger / ignition pulse (interrupt)
#define PIN_RPM           7

// ---------------------------------------------------------------------------
// Injector intercept — IRLZ44N MOSFETs
// ECU injector signal → MOSFET gate → injector
// Teensy controls gate to extend/trim pulse width
// ---------------------------------------------------------------------------
#define PIN_INJ1          2
#define PIN_INJ2          3
#define PIN_INJ3          4
#define PIN_INJ4          5

// Max trim range (fraction of pulse width, ±)
#define INJ_TRIM_MAX      0.15f   // ±15% max correction

// ---------------------------------------------------------------------------
// MAF intercept — frequency signal synthesis
// Teensy reads stock MAF frequency, applies displacement correction,
// outputs corrected frequency to ECU
// ---------------------------------------------------------------------------
#define PIN_MAF_IN        20    // Stock MAF frequency input
#define PIN_MAF_OUT       21    // Corrected frequency output to ECU
#define MAF_DISPLACEMENT_FACTOR  1.124f  // 2.6L / 2.3L = 1.130, fine tune on dyno

// ---------------------------------------------------------------------------
// CAN bus — TJA1051 transceivers
// ---------------------------------------------------------------------------
#define CAN1_TX           22    // CAN1 → OBD2 port
#define CAN1_RX           23
#define CAN2_TX           1     // CAN2 → gauge cluster / datalogger
#define CAN2_RX           0
#define CAN_BAUD          500000

// CAN frame IDs (arbitrary, change to suit your gauge)
#define CAN_ID_SENSORS    0x100
#define CAN_ID_CORRECTIONS 0x101
#define CAN_ID_STATUS     0x102

// ---------------------------------------------------------------------------
// SD / Datalogger
// ---------------------------------------------------------------------------
#define SD_CS             BUILTIN_SDCARD
#define DATALOG_FILENAME  "datalog.csv"
#define DATALOG_INTERVAL_MS  100   // Log every 100ms (10Hz)

// ---------------------------------------------------------------------------
// Corrections
// ---------------------------------------------------------------------------
// Closed loop fuel trim limits
#define FUEL_TRIM_MAX     0.10f   // ±10% max closed loop correction
#define FUEL_TRIM_STEP    0.002f  // Correction step size per cycle

// Knock retard
#define KNOCK_RETARD_DEG  2.0f    // Degrees retard per knock event
#define KNOCK_RETARD_MAX  10.0f   // Max total retard
#define KNOCK_RECOVER_DEG 0.5f    // Degrees advance recovery per cycle (no knock)
