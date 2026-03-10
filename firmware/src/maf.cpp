// =============================================================================
// maf.cpp — MAF sensor intercept + signal translation
//
// Strategy:
//   Teensy sits between MAF sensor and ECU.
//   Reads actual airflow from input sensor, translates to equivalent signal
//   the ECU expects from the stock 7A hotwire MAF, outputs corrected signal.
//
// Phase 1 — Stock 7A hotwire MAF (frequency output):
//   Input:  Stock MAF frequency signal on PIN_MAF_IN
//   Output: Corrected frequency on PIN_MAF_OUT (displacement correction only)
//   Correction: multiply frequency by MAF_DISPLACEMENT_FACTOR (2.6/2.3 = 1.13)
//   This compensates for larger displacement pulling more air per rev.
//
// Phase 2 — VW MK4 1.8T MAF (Bosch 0 280 218 037, analog 0–5V):
//   Input:  1.8T MAF analog voltage on PIN_MAF_IN_ANALOG (ADC)
//   Output: Equivalent 7A ECU MAF voltage on PIN_MAF_OUT_DAC (DAC)
//           OR frequency synthesis if ECU expects frequency
//   Translation: 1.8T voltage → kg/h (via 1.8T cal table)
//                kg/h → 7A ECU voltage (via 7A ECU cal table)
//
// The firmware is sensor-agnostic: swap MAF_INPUT_TYPE in config.h,
// update the input calibration table, everything else stays the same.
//
// Bosch 0 280 218 037 (MK4 1.8T) transfer curve:
//   Measured data from VAG-COM logs and Bosch datasheet interpolation.
//   Voltage range: 0.2V (idle/no flow) to 4.8V (max flow ~480 kg/h)
//   Output type: Analog voltage (simple ADC read)
//
// 7A ECU MAF expectation (893906266D):
//   Stock 7A Hitachi hotwire MAF outputs a frequency signal.
//   Frequency range: ~10 Hz (idle) to ~150 Hz (max load)
//   ECU reads frequency via capture timer.
//   Teensy replicates this on PIN_MAF_OUT using hardware PWM/timer.
// =============================================================================

#include "maf.h"
#include "config.h"

// ---------------------------------------------------------------------------
// MAF input type selection (set in config.h)
// MAF_INPUT_FREQUENCY = stock 7A hotwire (Phase 1)
// MAF_INPUT_ANALOG    = MK4 1.8T hot film (Phase 2)
// ---------------------------------------------------------------------------
#ifndef MAF_INPUT_TYPE
  #define MAF_INPUT_TYPE MAF_INPUT_FREQUENCY   // Default: Phase 1
#endif

// ---------------------------------------------------------------------------
// 1.8T MAF transfer curve (Bosch 0 280 218 037)
// Format: {voltage * 100 (uint16), airflow kg/h * 10 (uint16)}
// Source: VAG-COM datalog correlation + Bosch application data
// ---------------------------------------------------------------------------
static const uint16_t maf18T_table[][2] = {
  {  20,    0 },   // 0.20V →   0.0 kg/h (no flow / sensor idle)
  {  50,   15 },   // 0.50V →   1.5 kg/h
  { 100,   50 },   // 1.00V →   5.0 kg/h
  { 150,  120 },   // 1.50V →  12.0 kg/h
  { 175,  200 },   // 1.75V →  20.0 kg/h (idle ~600 RPM)
  { 200,  310 },   // 2.00V →  31.0 kg/h
  { 225,  450 },   // 2.25V →  45.0 kg/h
  { 250,  620 },   // 2.50V →  62.0 kg/h
  { 275,  830 },   // 2.75V →  83.0 kg/h
  { 300, 1080 },   // 3.00V → 108.0 kg/h
  { 325, 1370 },   // 3.25V → 137.0 kg/h
  { 350, 1700 },   // 3.50V → 170.0 kg/h
  { 375, 2080 },   // 3.75V → 208.0 kg/h
  { 400, 2500 },   // 4.00V → 250.0 kg/h
  { 425, 2970 },   // 4.25V → 297.0 kg/h
  { 450, 3490 },   // 4.50V → 349.0 kg/h
  { 475, 4050 },   // 4.75V → 405.0 kg/h
  { 480, 4300 },   // 4.80V → 430.0 kg/h (near max)
};
static const int maf18T_tableSize = sizeof(maf18T_table) / sizeof(maf18T_table[0]);

// ---------------------------------------------------------------------------
// 7A ECU MAF output table
// What frequency does the 7A ECU expect for a given airflow?
// Format: {airflow kg/h * 10 (uint16), frequency Hz * 10 (uint16)}
// Source: Oscilloscope measurements on stock 7A + 034 RIP chip documentation
// NOTE: These values are approximated — VERIFY with oscilloscope on your car
//       before enabling Phase 2. Log stock MAF freq vs RPM/load first.
// ---------------------------------------------------------------------------
static const uint16_t maf7A_table[][2] = {
  {    0,   80 },   //   0.0 kg/h →   8.0 Hz  (engine off baseline)
  {   15,  100 },   //   1.5 kg/h →  10.0 Hz
  {   50,  140 },   //   5.0 kg/h →  14.0 Hz  (idle)
  {  120,  200 },   //  12.0 kg/h →  20.0 Hz
  {  200,  280 },   //  20.0 kg/h →  28.0 Hz  (light load)
  {  310,  380 },   //  31.0 kg/h →  38.0 Hz
  {  450,  500 },   //  45.0 kg/h →  50.0 Hz
  {  620,  640 },   //  62.0 kg/h →  64.0 Hz
  {  830,  790 },   //  83.0 kg/h →  79.0 Hz
  { 1080,  960 },   // 108.0 kg/h →  96.0 Hz
  { 1370, 1140 },   // 137.0 kg/h → 114.0 Hz
  { 1700, 1330 },   // 170.0 kg/h → 133.0 Hz  (WOT ~3000 RPM NA)
  { 2080, 1520 },   // 208.0 kg/h → 152.0 Hz
  { 2500, 1700 },   // 250.0 kg/h → 170.0 Hz  (WOT ~4500 RPM NA)
  { 3000, 1900 },   // 300.0 kg/h → 190.0 Hz  (turbo territory)
  { 4000, 2200 },   // 400.0 kg/h → 220.0 Hz
  { 4300, 2300 },   // 430.0 kg/h → 230.0 Hz  (sensor max)
};
static const int maf7A_tableSize = sizeof(maf7A_table) / sizeof(maf7A_table[0]);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static float   mafAirflow_kgh   = 0.0f;    // Current measured airflow kg/h
static float   mafInputVoltage  = 0.0f;    // Phase 2: 1.8T sensor voltage
static float   mafOutputFreq_Hz = 0.0f;    // Current output frequency to ECU

// Phase 1: frequency measurement via interrupt
static volatile uint32_t mafLastPulseUs  = 0;
static volatile uint32_t mafPulseInterval = 0;
static volatile bool     mafNewPulse     = false;

// Phase 1: output frequency via hardware timer (IntervalTimer)
static IntervalTimer mafOutputTimer;
static volatile bool mafOutputPin = false;

// ---------------------------------------------------------------------------
// Interpolation helper
// Inputs are scaled integers: x in table_x col, result in table_y col
// ---------------------------------------------------------------------------
static float interpolate(const uint16_t table[][2], int tableSize,
                         float x, int xCol, int yCol) {
  if (x <= table[0][xCol])
    return (float)table[0][yCol];
  if (x >= table[tableSize-1][xCol])
    return (float)table[tableSize-1][yCol];

  for (int i = 0; i < tableSize - 1; i++) {
    if (x >= table[i][xCol] && x < table[i+1][xCol]) {
      float frac = (x - table[i][xCol]) /
                   (float)(table[i+1][xCol] - table[i][xCol]);
      return table[i][yCol] + frac * (table[i+1][yCol] - table[i][yCol]);
    }
  }
  return (float)table[tableSize-1][yCol];
}

// ---------------------------------------------------------------------------
// Phase 1: ISR — stock 7A MAF frequency input
// ---------------------------------------------------------------------------
FASTRUN static void maf_input_isr() {
  uint32_t now = micros();
  mafPulseInterval = now - mafLastPulseUs;
  mafLastPulseUs   = now;
  mafNewPulse      = true;
}

// ---------------------------------------------------------------------------
// Phase 1: Output timer ISR — toggle PIN_MAF_OUT to synthesise frequency
// ---------------------------------------------------------------------------
FASTRUN static void maf_output_timer_isr() {
  mafOutputPin = !mafOutputPin;
  digitalWriteFast(PIN_MAF_OUT, mafOutputPin);
}

// ---------------------------------------------------------------------------
// setOutputFrequency() — update IntervalTimer period
// Frequency synthesis: toggle pin at 2× frequency (half period)
// ---------------------------------------------------------------------------
static void setOutputFrequency(float freq_Hz) {
  if (freq_Hz < 1.0f) freq_Hz = 1.0f;
  uint32_t halfPeriodUs = (uint32_t)(500000.0f / freq_Hz);  // µs
  mafOutputTimer.update(halfPeriodUs);
}

// ---------------------------------------------------------------------------
// updatePhase1() — stock 7A MAF frequency intercept
// Reads stock MAF frequency, applies displacement correction, outputs to ECU
// ---------------------------------------------------------------------------
static void updatePhase1() {
  if (mafNewPulse) {
    mafNewPulse = false;
    if (mafPulseInterval > 0) {
      float inputFreq = 1000000.0f / (float)mafPulseInterval;
      // Apply displacement correction (2.6L draws more air per rev than 2.3L)
      // ECU sees corrected frequency as if stock 2.3L is pulling that air
      float outputFreq = inputFreq * MAF_DISPLACEMENT_FACTOR;
      outputFreq = constrain(outputFreq, 1.0f, 500.0f);
      setOutputFrequency(outputFreq);
      mafOutputFreq_Hz = outputFreq;

      // Rough airflow estimate from frequency for datalog
      // Stock 7A: ~1.5 kg/h per Hz (approximate, calibrate on dyno)
      mafAirflow_kgh = inputFreq * 1.5f;
    }
  }

  // Stale — MAF signal lost
  if (micros() - mafLastPulseUs > 500000UL) {
    mafAirflow_kgh  = 0.0f;
    mafOutputFreq_Hz = 8.0f;   // Idle baseline
    setOutputFrequency(8.0f);
  }
}

// ---------------------------------------------------------------------------
// updatePhase2() — 1.8T analog MAF → 7A ECU frequency synthesis
// ---------------------------------------------------------------------------
static void updatePhase2() {
  // Read 1.8T MAF analog voltage (0–5V through 10k/20k divider → 0–3.3V)
  int raw = analogRead(PIN_MAF_IN);
  float v_teensy = (raw / 4095.0f) * 3.3f;
  float v_sensor = v_teensy / 0.6667f;   // Undo divider
  mafInputVoltage = constrain(v_sensor, 0.2f, 4.8f);

  // 1.8T voltage → airflow kg/h
  float v_scaled   = mafInputVoltage * 100.0f;   // Match table scale
  float flow_scaled = interpolate(maf18T_table, maf18T_tableSize,
                                   v_scaled, 0, 1);
  mafAirflow_kgh = flow_scaled / 10.0f;   // Undo table scale → kg/h

  // Airflow kg/h → 7A ECU frequency
  float flow_lookup = mafAirflow_kgh * 10.0f;    // Match table scale
  float freq_scaled  = interpolate(maf7A_table, maf7A_tableSize,
                                    flow_lookup, 0, 1);
  float outputFreq   = freq_scaled / 10.0f;       // Hz

  outputFreq = constrain(outputFreq, 1.0f, 500.0f);
  setOutputFrequency(outputFreq);
  mafOutputFreq_Hz = outputFreq;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void maf_init() {
  analogReadResolution(12);

#if MAF_INPUT_TYPE == MAF_INPUT_FREQUENCY
  // Phase 1: frequency input + frequency output
  pinMode(PIN_MAF_IN,  INPUT);
  pinMode(PIN_MAF_OUT, OUTPUT);
  digitalWriteFast(PIN_MAF_OUT, LOW);

  attachInterrupt(digitalPinToInterrupt(PIN_MAF_IN), maf_input_isr, RISING);

  // Start output timer at idle frequency
  mafOutputTimer.begin(maf_output_timer_isr, 62500);  // 8 Hz default
  Serial.println(F("  MAF: Phase 1 — frequency intercept (stock 7A sensor)"));
  Serial.print(F("  Displacement correction factor: "));
  Serial.println(MAF_DISPLACEMENT_FACTOR, 4);

#elif MAF_INPUT_TYPE == MAF_INPUT_ANALOG
  // Phase 2: analog input + frequency output
  pinMode(PIN_MAF_IN,  INPUT);   // ADC input
  pinMode(PIN_MAF_OUT, OUTPUT);
  digitalWriteFast(PIN_MAF_OUT, LOW);

  mafOutputTimer.begin(maf_output_timer_isr, 62500);  // 8 Hz default
  Serial.println(F("  MAF: Phase 2 — 1.8T analog → 7A frequency synthesis"));
  Serial.println(F("  WARNING: Verify 7A frequency table with oscilloscope before use"));
#endif
}

void maf_update() {
#if MAF_INPUT_TYPE == MAF_INPUT_FREQUENCY
  updatePhase1();
#elif MAF_INPUT_TYPE == MAF_INPUT_ANALOG
  updatePhase2();
#endif

#ifdef DEBUG_SENSORS
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    Serial.print(F("MAF: "));
    Serial.print(mafAirflow_kgh, 1);
    Serial.print(F(" kg/h → "));
    Serial.print(mafOutputFreq_Hz, 1);
    Serial.println(F(" Hz out"));
  }
#endif
}

float maf_getAirflow_kgh()   { return mafAirflow_kgh; }
float maf_getOutputFreq_Hz() { return mafOutputFreq_Hz; }
float maf_getInputVoltage()  { return mafInputVoltage; }
