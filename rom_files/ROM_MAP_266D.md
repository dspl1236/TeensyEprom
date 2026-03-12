# 893906266D (7A Late) — Full ROM Map

Derived from physical chip read (MMS-05C HF201Y43) + 034 RIP Chip .ecu analysis.
ROM size: 32768 bytes (27C256). All addresses are native (post-unscramble for .034 files).

## CONFIRMED MAPS (documented by 034 RIP Chip)

| Address | Size | Description | Formula | Notes |
|---------|------|-------------|---------|-------|
| 0x0000 | 16×16 | **Fuel Map** | signed+128 | Rows=RPM, Cols=Load kPa. Stock 40–123 |
| 0x0100 | 16×16 | **Timing Map** | raw° BTDC | >128 = retard (signed wrap). Stock 0–36° |
| 0x0250 | 1×16 | **Fuel RPM Axis** | ×25=RPM | 600–6300 RPM |
| 0x0260 | 1×16 | **Load Axis (fuel)** | ×0.3922=kPa | 12.6–100.0 kPa |
| 0x0270 | 1×16 | **Timing RPM Axis** | ×25=RPM | 700–6300 RPM (differs from fuel) |
| 0x0280 | 1×16 | **Load Axis (timing)** | ×0.3922=kPa | 12.6–100.0 kPa |
| 0x0640 | 1×16 | **CL Load Axis** | ×25=RPM? | Closed loop RPM axis |
| 0x0660 | 1×16 | **CL Load Limit** | raw | Closed loop load threshold |
| 0x077E | scalar | **Injection Scaler** | raw | Stock=100 |
| 0x07E1 | scalar | **CL Disable RPM** | ×25=RPM | Stock=244 → 6100 RPM |
| 0x0E20 | 1×16 | **Decel RPM Axis** | ×25=RPM | 1000–6375 RPM |
| 0x0E30 | 1×16 | **Decel Cutoff** | ×0.3922=kPa | Injector cut threshold vs RPM |
| 0x1000 | 16×16 | **Knock Safety Timing** | raw° BTDC | ECU falls back here on knock |
| 0x1600 | 512 | **Checksum Correction** | — | Target sum: 3,384,576 (0x33A500) |

## UNDOCUMENTED REGIONS (identified from ROM analysis)

### Axes / Lookup Tables

| Address | Size | Description | Decoded values |
|---------|------|-------------|----------------|
| 0x024E | 1×16 | Unknown RPM axis (×25) | starts at 200 RPM |
| 0x0290 | 1×16 | **Idle/low-RPM axis** (×25) | 400,500,600,700,800,900,1000,1100,1200,1400,1600,1800,2000,2400,2800,3600 |
| 0x02D0 | 32×16-bit BE | **MAF Linearization** (same addr as 266B!) | 1,3,5,8,10,13,18,23...854 — MAF voltage→g/s table |
| 0x0350 | 1×16 | RPM axis (×25) | 250,500,750...4000 RPM (16 evenly-spaced) |
| 0x05C0 | 1×16 | Unknown axis (appears 3×: 0x05C0, 0x0940, 0x0B70) | 5,15,25...140 — possibly TPS% or load % |
| 0x0630 | 1×16 | CL load axis (% or kPa units) | 63,75,88,100,113...250 |
| 0x06C0 | 1×16 | RPM axis (×25) | 250,500,750...4000 RPM |

### 1-D Maps (likely enrichment/correction)

| Address | Size | Description | Notes |
|---------|------|-------------|-------|
| 0x06D0 | 1×16 | **Warmup/idle fuel enrichment vs RPM** | 200,170,130,106×8,98×5 — high at idle, tapers off. Axis at 0x06C0 |
| 0x06E0 | 1×16 | **Idle timing correction row 1** | ~90–96°, nearly flat. Two identical copies (0x06E0, 0x06F0) + variant at 0x0700 |
| 0x0710 | 1×16 | **Idle timing/dwell table** | 40,40,40,38,36,35,34...32. Identical × 3 (0x0710, 0x0720, 0x0730) |
| 0x0740 | 1×16 | **Timing correction ramp** | 32,32,32,30,28,26,24...16 |
| 0x06C0+0x06D0 | paired | **CL enrichment 1-D map** | RPM axis + data |

### 2-D Maps (undocumented, likely corrections/enrichment)

| Address | Size | Description | Notes |
|---------|------|-------------|-------|
| 0x0200 | ~5×16 | **Unknown 0x200 area** | Contains tune differences (0x0204/0x0214 differ from 034 stock) — possibly cold-start or IAT correction |
| 0x0360–0x03E0 | multiple 1×16 | **Overrun/decel enrichment curves** | Multiple rows: ramp-down tables 105,87,69,51...0; signed retard 252,252...249 |
| 0x0400–0x0470 | multiple 1×16 | **More enrichment curves** | 64,64,60,56...52; 17,16,15...14; 93,89,83...4; 245,244,243...229 |
| 0x0550–0x0590 | 4×16 | **Decel enrichment map** | Values 255,217,142,75...12 — sharply decreasing, looks like overrun fuel cut vs load |
| 0x0800–0x0880 | 8×8 blocks | **Cranking/start enrichment maps** | Multiple 8-entry tables with increasing values |
| 0x08F0 | 1×16 | **O2/lambda correction curve?** | 255,220,197,178...0 — monotonic decrease |
| 0x0A10–0x0A70 | 6×16 | **Idle air / throttle control map** | Values 128,192,200,184,176 — step-wise, likely idle air valve duty |
| 0x0C00–0x0C20 | 2×16 | **Physical chip tune region** | Differs from 034D (11 vs 12), possibly cylinder balance or idle correction |
| 0x0E40 | 2×8 | **Second decel map** | 36,40,48,56,64,80,96,112 + 48,60,72,84,99,114,129,144 |
| 0x1120 | 1×16 | **Calibration table (266D only)** | 8,16,23,31,47,63...219 — evenly stepped, ~3-86% of 255. O2 sensor gain? |
| 0x1130 | 1×16 | **Calibration table (266D only)** | 12,20,27,39,55,71...227 — similar to 0x1120, offset. Paired lookup? |
| 0x11A0–0x11C0 | 3×16 | **Three lambda/fuel correction rows** | ~85–104, slight dropoff at high RPM. Could be per-cylinder or per-condition trim |
| 0x11F0–0x1210 | 2×16 | **O2 target/trim curve** | 128×5 then ramps down to 56 — 128=midpoint, ramp = lean target at high RPM? |

### MAF Linearization (0x02D0) — Same address as 266B!

The 266D contains a 32-entry 16-bit big-endian MAF linearisation table at 0x02D0,
identical to the one documented in the 266B .ecu. Values: 1,3,5,8,10,13,18,23,26,30,
35,40,47,55,67,80,95,110,135,160,195,230,274,318,371,424,486,548,619,691,772,854.
These represent MAF sensor frequency/voltage → airflow (g/s × some factor).
034 did not expose this in their tuner tool for the 266D, possibly because
they treat it as a fixed calibration, not a tuning parameter.

## CHECKSUM

- Target: 3,384,576 (0x33A500)
- Correction region: 0x1600–0x17FF (512 bytes)
- Algorithm: byte sum, distribute remainder across correction region

## RESET VECTOR

- Address: 0x7FFE–0x7FFF
- Value: 0xE8 0xB1 (→ CPU start address 0xB1E8)
- Used for ECU version detection (266D-specific)

