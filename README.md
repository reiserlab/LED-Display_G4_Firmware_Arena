# G4.1 Arena Skeleton

Reimplements the G4.1-ArenaController for Teensy 4.1 without the QP/state-machine framework. Uses a simple main loop with state tracking.

- TCP command server (QNEthernet, port 62222, G4 binary protocol).
- Pattern file playback from SD card (SdFat, SDIO).
- Two SPI buses driving LED panels.

## Build

Requires [PlatformIO](https://platformio.org/) managed via [pixi](https://pixi.sh/).

```
pixi run deploy      # compile and upload
```

## Source files

All source files live in `src/`.

| File | Purpose |
|------|---------|
| `main.cpp` | Setup, main loop, interrupt priorities |
| `NetworkManager.h/.cpp` | TCP server, G4 binary protocol parsing, response buffer |
| `SdManager.h/.cpp` | SD card init, pattern directory scan, frame reads |
| `SpiManager.h/.cpp` | Dual SPI bus setup, panel select matrix, frame transfers |
| `CommandProcessor.h/.cpp` | Arena state machine, command handling, pattern playback timing |
| `constants.h` | Hardware constants, panel geometry, timing, protocol values |
| `commands.h` | `ArenaCommands` enum |
| `modes.h` | `ControlModes` enum |
| `PatternHeader.h` | 7-byte pattern file header struct |

## SD card pattern files

Pattern files must be placed in `/patterns/` on the SD card. At startup, `SdManager` scans this directory and builds an alphabetically sorted list. Pattern IDs (1-based) map to position in that sorted list.

### Limitations

- **Maximum 10,000 files.** Files beyond this limit are silently ignored.
- **Filename length.** Names are truncated to 32 characters. Files whose names differ only after the 32nd character will have nondeterministic ordering.
- **Hidden files and subdirectories are skipped.** Any entry starting with `.` or any subdirectory inside `/patterns/` is excluded from the scan.
- **Scan runs once at startup.** Adding or removing files on the SD card requires a reboot to update the pattern list.
