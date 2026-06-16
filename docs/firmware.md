---
title: G4.1 Arena Firmware
parent: Assembly
grand_parent: Generation 4
nav_order: 2
---

1. TOC
{:toc}

# Arena Firmware (G4.1)

The G4.1 arena ([Hardware Arena 12-12 v10]({{$site.baseurl}}/Generation%204/Arena/docs/arena.html#a12-12v10)) runs custom firmware on a [Teensy 4.1](https://www.pjrc.com/store/teensy41.html) microcontroller. It reimplements the original [ArenaController](https://github.com/janelia-arduino/ArenaController) without the QP/state-machine framework, using a simple main loop with state tracking and approximately one quarter of the original code volume.

The firmware provides three core functions:

- **TCP command server** — listens on port 62222 and speaks the G4 binary protocol (QNEthernet).
- **Pattern playback from SD card** — reads pattern files via SDIO (SdFat).
- **Two SPI buses driving LED panels** — independent buses for the two halves of the arena.

## Prerequisites

[pixi](https://pixi.sh/): follow the instructions from the pixi website to install pixi for your platform (Windows, MacOS, Linux). Pixi handles all the other dependencies.

To use the SD card patterns, the Teensy needs to have an SD card with a FAT32 volume named `PATSD` and the largest cluster size you can select during SD card formatting.

## Build and deploy

From the repository root (`Generation 4/Firmware-Arena/`):

```
pixi run deploy         # compile and upload
```

## Hardware configuration

| Parameter | Value |
|-----------|-------|
| SPI clock | 5 MHz, MSB-first, MODE0 |
| SPI buses | SPI (CIPO pin 12) and SPI1 (CIPO pin 1) |
| Panel grid | 2 rows × 12 columns (6 per bus); up to 5 × 12 max |
| Display refresh | 300 Hz (greenscale) / 1,000 Hz (binary) default |
| Network | DHCP, port 62222 |

## SD card pattern files

Pattern files must be placed in `/patterns/` on the SD card. The name of the FAT volume for the SD card needs to be `PATSD`. At startup, the Teensy scans this directory and builds an alphabetically sorted list. Pattern IDs (1-based) map to position in that sorted list. Once the patterns are initialized, the Teensy blinks with the Morse Code for "OK".

### RAM caching

When a pattern file fits in available heap minus a 32 KB reserve (kept free for incoming network packets), the entire file is loaded into RAM when the file is opened. Frame reads then become a `memcpy` (~2 µs) instead of an SD read. For files that do not fit, reads go directly to the SD card with a pre-warmed FAT cache, which results in a ~600 µs read delay at FAT cluster boundaries. To minimize this delay, format the SD card with the largest available cluster size.

### Limitations

- **Maximum 10,000 files.** Files beyond this limit are silently ignored.
- **Filename length.** Names are truncated to 32 characters. Files whose names differ only after the 32nd character will have nondeterministic ordering.
- **Hidden files and subdirectories are skipped.** Any entry starting with `.`, and any subdirectory inside `/patterns/`, is excluded from the scan.
- **Scan runs once at startup.** Adding or removing files on the SD card requires a reboot to update the pattern list. A directory containing about 1000 files takes approximately 2.2 s to scan.

## Measured performance

### SD-based pattern playback

Tested across two runs (5 and 16 sessions, 4,165 and 13,455 frames, up to 80 s of continuous operation at inter-frame cadences from 3 ms to 55 ms):

- **Zero dropped frames** in both runs.
- **Timing accuracy:** 3 minor single-block jitter events (+2 ms each) out of 4,831 timing blocks in the 16-session run (0.06 %). No frame was skipped; jitter resolved within one block.
- **Session duration drift:** ≤ 4 ms over 5,000 ms sessions (~160 ppm worst case).
- **Pattern switching latency:** 1–19 ms depending on file size. Cached small files open in 1–7 ms; large files (skipping full pre-load) open in < 1 ms. Total overhead across 15 consecutive transitions: 61 ms.
- **Cached frame reads:** ~2 µs per frame (pure `memcpy`).

### TCP streaming

**300 Hz greenscale** (expected inter-frame interval 3.33 ms) — over 4,521 streamed frames across three sessions:

- 12 drops total (0.27 % drop rate); all drops are single-frame misses (6 ms gap = exactly one missed period).

**~3,000 Hz binary** — ~2,900 fps delivered over two 5 s sessions (29,129 frames). The 1,000 Hz display refresh rate means approximately every third frame was actually displayed.

- Binary streaming (gs = 0) showed no drops.
