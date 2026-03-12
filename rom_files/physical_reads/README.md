# Physical Chip Reads

## 893906266D_MMS05C_physical.bin

- **Source:** Physical EPROM desoldered from Hitachi GE002336-2 ECU board (893906266D)
- **Chip markings:** MMS-05C  HF201Y43
- **Chip type:** 27C256A-20-XG (32KB, 200ns, 28-pin DIP)
- **Read with:** T48 TL866-3G (XGPro, device: 27C256, 5V)
- **Size:** 32768 bytes
- **CRC32:** 0x4152E167
- **ECU detection:** 266D HIGH confidence (reset vector E8 B1 @ 0x7FFE)
- **Checksum:** Does NOT match stock 034 target — this ECU had a tune on it

### Differences vs 034 stock file

1098 bytes differ across 20 regions. The fuel map and timing map cores are
**identical** to the 034 stock file. Differences are in:

| Region | Bytes | Likely function |
|--------|-------|----------------|
| 0x0204–0x021B | 16 | Unknown 0x200 region (16→19) |
| 0x0628 | 1 | CL Load limit (153→195) |
| 0x07C8 | 1 | Scalar in 0x700 region (25→19) |
| 0x0846 | 1 | Scalar in 0x800 region (16→8) |
| 0x0C09–0x0C1D | 10 | 0xC00 region maps (11→12) |
| 0x0E56–0x0E8D | 37 | Decel/overrun enrichment maps |
| 0x11DE, 0x1335 | 2 | Knock map tweaks |
| 0x13FB–0x17FF | 1029 | Checksum correction region |
| 0x7FE1–0x7FE9 | 4 | Near reset vector (code patches?) |

**Conclusion:** This chip contains a previously programmed tune (not factory OEM,
not identical to 034's "stock" file). The 034 stock file was likely sourced from
a different ECU revision or hardware lot. The physical chip represents the actual
tune that ran in this specific 7A engine before removal.

