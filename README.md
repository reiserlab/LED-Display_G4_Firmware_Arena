# G4.1 Arena Skeleton

Reimplements the G4.1-ArenaController for Teensy 4.1 without the QP/state-machine framework. Uses a simple main loop with state tracking.

- TCP command server (QNEthernet, port 62222, G4 binary protocol).
- Pattern file playback from SD card (SdFat, SDIO).
- Two SPI buses driving LED panels.

## Build

Requires [PlatformIO](https://platformio.org/) managed via [pixi](https://pixi.sh/).

```
pixi run deploy         # compile and upload
pixi run deploy-printf  # compile with printf, and upload
```

## Source files

All source files live in `src/`.

| File | Purpose |
|------|---------|
| `main.cpp` | Setup, main loop, interrupt priorities |
| `NetworkManager.h/.cpp` | TCP server, G4 binary protocol parsing, response buffer |
| `SdManager.h/.cpp` | SD card init, pattern directory scan, RAM caching, frame reads |
| `SpiManager.h/.cpp` | Dual SPI bus setup, panel select matrix, frame transfers |
| `CommandProcessor.h/.cpp` | Arena state machine, command handling, pattern playback timing |
| `constants.h` | Hardware constants, panel geometry, timing, protocol values |
| `commands.h` | `ArenaCommands` enum |
| `modes.h` | `ControlModes` enum |
| `PatternHeader.h` | 7-byte pattern file header struct (the `grayscale_value` field name is inherited from the G4 binary protocol; the project calls the mode "greenscale") |

## Hardware configuration

- **SPI clock:** 5 MHz, MSB-first, MODE0
- **Regions:** SPI (CIPO pin 12) + SPI1 (CIPO pin 1)
- **Panel grid:** 2 rows × 12 cols (6 per region), up to 5×12 max
- **Display refresh:** 300 Hz (greenscale) / 1000 Hz (binary) default
- **Network:** DHCP on port 62222

## SD card pattern files

Pattern files must be placed in `/patterns/` on the SD card. At startup, `SdManager` scans this directory and builds an alphabetically sorted list. Pattern IDs (1-based) map to position in that sorted list.

### RAM caching

When a pattern file fits in available heap minus a 32 KB reserve (kept free for incoming network packets), the entire file is loaded into RAM at file open time. Frame reads then become a `memcpy` (~2 µs) instead of an SD read. For files that do not fit, reads go to the SD card with a pre-warmed FAT cache, which results in a ~600 µs read delay at FAT cluster boundaries. To minimize this, format the SD card with the largest available cluster size.

### Limitations

- **Maximum 10,000 files.** Files beyond this limit are silently ignored.
- **Filename length.** Names are truncated to 32 characters. Files whose names differ only after the 32nd character will have nondeterministic ordering.
- **Hidden files and subdirectories are skipped.** Any entry starting with `.` or any subdirectory inside `/patterns/` is excluded from the scan.
- **Scan runs once at startup.** Adding or removing files on the SD card requires a reboot to update the pattern list. A 743-file directory takes approximately 2.2 s to scan.

## Measured performance

### SD-based pattern playback

Tested across two runs (5 and 16 sessions, 4,165 and 13,455 frames, up to 80 s continuous operation at cadences from 3 ms to 55 ms per frame):

- **Zero dropped frames** in both runs.
- **Timing accuracy:** 3 minor single-block jitter events (+2 ms each) out of 4,831 timing blocks in the 16-session run (0.06 %). No frame was skipped; the jitter resolved within one block.
- **Session duration drift:** ≤ 4 ms over 5,000 ms sessions (~160 ppm worst-case).
- **Pattern switching latency:** 1–19 ms depending on file size. Cached small files open in 1–7 ms; large files (skipping full pre-load) in < 1 ms. Total overhead across 15 consecutive transitions: 61 ms.
- **Cached frame reads:** ~2 µs per frame (pure `memcpy`).

### TCP streaming (300 Hz greenscale, ~3,000 Hz binary)

At 300 Hz the expected inter-frame interval is 3.33 ms. Over 4,521 streamed frames across three sessions:

- **12 drops total** (0.27 % drop rate); all drops are single-frame misses (6 ms gap = exactly one missed period).

At ~3,000 Hz the network delivered ~2,900 fps (29,129 frames over two 5 s sessions), but the 1,000 Hz display refresh rate means approximately every 3rd frame was actually displayed.

- **Binary streaming** (gs=0) showed no drops.
