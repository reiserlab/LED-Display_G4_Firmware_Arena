---
title: G4.1 Arena Firmware
parent: Assembly
grand_parent: Generation 4
nav_order: 2
---

1. TOC
{:toc}

# Arena Firmware (G4.1)

The G4.1 arena ([Hardware Arena 12-12 v10]({{site.baseurl}}/Generation%204/Arena/docs/arena.html#a12-12v10)) runs custom firmware on a [Teensy 4.1](https://www.pjrc.com/store/teensy41.html) microcontroller. It reimplements the original [ArenaController](https://github.com/janelia-arduino/ArenaController) without the QP/state-machine framework, using a simple main loop with state tracking and approximately one quarter of the original code volume.

The firmware provides three core functions:

- **TCP command server:** listens on port 62222 and speaks the G4 binary protocol (QNEthernet).
- **Pattern playback from SD card:** reads pattern files via SDIO (SdFat).
- **Two SPI buses driving LED panels:** independent buses for the two halves of the arena.

## Prerequisites

The build system uses [pixi](https://pixi.sh/), which manages all other dependencies (including the compiler and PlatformIO) automatically. Follow the instructions on the pixi website to install it for your platform (Windows, macOS, or Linux). You do not need to install PlatformIO separately.

To display patterns from the SD card, the Teensy needs an SD card with a FAT32 volume named `PATSD`. When formatting the card, choose the largest available cluster size to minimize read latency during playback (see [RAM caching](#ram-caching) for details).

## Updating the firmware

This section describes how to compile and upload the arena firmware to the Teensy. You would typically do this when setting up a new arena, after pulling in a firmware update, or after making your own changes to the source code.

### 1. Clone the repository

Clone the public repository (or your fork of it) to your local machine and navigate to the arena firmware directory:

```
cd "Generation 4/Firmware-Arena"
```

### 2. Connect the Teensy

Connect the Teensy 4.1 to your computer via USB. Make sure no other process is using its serial port.

### 3. Compile and upload

Run the following command from the `Generation 4/Firmware-Arena/` directory:

```
pixi run deploy
```

Pixi will automatically download the compiler toolchain and all required libraries the first time it runs, which may take a few minutes. On subsequent runs it will use the cached tools and compile much faster. The command compiles the firmware and uploads it directly to the Teensy over USB.

Once the upload is complete, the Teensy will reboot, initialize the SD card, and scan the pattern directory. When ready, it signals success by blinking the on-board LED with the Morse code for "OK".

## Hardware configuration

The following fixed hardware parameters are compiled into the firmware. If you need to change any of them (for example, to support a different panel grid size), edit `src/constants.h` and redeploy.

| Parameter | Value |
|-----------|-------|
| SPI clock | 5 MHz, MSB-first, MODE0 |
| SPI buses | SPI (CIPO pin 12) and SPI1 (CIPO pin 1) |
| Panel grid | 2 rows x 12 columns (6 per bus); up to 5 x 12 max |
| Display refresh | 300 Hz (greenscale) / 1,000 Hz (binary) default |
| Network | DHCP, port 62222 |

## SD card pattern files

Pattern files must be placed in a directory named `/patterns/` at the root of the SD card. The FAT volume label must be `PATSD`. At startup, the Teensy scans this directory and builds an alphabetically sorted list of all pattern files. Pattern IDs (1-based) map to the position in that sorted list: the first file alphabetically is pattern 1, the second is pattern 2, and so on. Once the patterns are initialized, the Teensy signals readiness by blinking the Morse code for "OK".

### RAM caching

To keep playback fast, the firmware tries to load each pattern file entirely into RAM when it is opened. If the file fits within the available heap (minus a 32 KB reserve kept free for incoming network packets), all subsequent frame reads become a simple memory copy (approximately 2 us per frame). If the file is too large to cache, frames are read directly from the SD card using a pre-warmed FAT cache; this is still reliable but introduces an approximately 600 us delay at FAT cluster boundaries. Formatting the SD card with the largest available cluster size reduces the frequency of these boundary crossings.

### Limitations

- **Maximum 10,000 files.** Files beyond this limit are silently ignored during the startup scan.
- **Filename length.** Names longer than 32 characters are truncated. Files whose names differ only after the 32nd character will have nondeterministic ordering.
- **Hidden files and subdirectories are skipped.** Any entry whose name starts with `.`, and any subdirectory inside `/patterns/`, is excluded from the scan.
- **Scan runs once at startup.** Adding or removing files on the SD card requires a reboot for the changes to take effect. A directory containing about 1,000 files takes approximately 2.2 s to scan.

## Measured performance

The following results were collected during bench testing and give a practical sense of what to expect in normal experimental use.

### SD-based pattern playback

The tests covered two runs (5 and 16 sessions, 4,165 and 13,455 frames, up to 80 s of continuous operation at inter-frame cadences from 3 ms to 55 ms):

- **Zero dropped frames** in both runs.
- **Timing accuracy:** 3 minor single-block jitter events (+2 ms each) out of 4,831 timing blocks in the 16-session run (0.06 %). No frame was skipped; jitter was resolved within one block.
- **Session duration drift:** 4 ms or less over 5,000 ms sessions (approximately 160 ppm worst case).
- **Pattern switching latency:** 1 to 19 ms depending on file size. Cached small files open in 1 to 7 ms; large files that skip full pre-loading open in less than 1 ms. Total overhead across 15 consecutive transitions: 61 ms.
- **Cached frame reads:** approximately 2 us per frame (pure `memcpy`).

### TCP streaming

At **300 Hz greenscale** (expected inter-frame interval 3.33 ms), results over 4,521 streamed frames across three sessions were as follows:

- 12 drops total (0.27 % drop rate); all drops are single-frame misses (6 ms gap corresponding to exactly one missed period).

At **approximately 3,000 Hz binary**, approximately 2,900 fps were delivered over two 5 s sessions (29,129 frames). The 1,000 Hz display refresh rate means approximately every third frame was actually displayed.

- Binary streaming (gs = 0) showed no drops.
