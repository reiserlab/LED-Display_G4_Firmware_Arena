# Teensy 4.1 Low-Latency TCP + SD + SPI Skeleton

Target: Teensy 4.1, Teensyduino/Arduino core.

This project provides:

- TCP command server using QNEthernet (TCP, low latency).
- SD access using SdFat on the built-in SDIO.
- Two SPI buses (SPI and SPI1) with high-priority handling.
- A simple command queue and processor.

## File list

- `README.md` — this file.
- `NetworkManager.h` / `NetworkManager.cpp` — TCP server + command queue + response buffer.
- `SdManager.h` / `SdManager.cpp` — SDIO-based SdFat helpers.
- `SpiManager.h` / `SpiManager.cpp` — SPI bus setup and basic transfer helpers.
- `CommandProcessor.h` / `CommandProcessor.cpp` — maps commands to SD/SPI operations.
- `main.cpp` — integrates all managers and sets interrupt priorities.

## Setup instructions

1. Create a new Arduino/Teensy project (or a new folder).
2. Install dependencies:
   - QNEthernet (via Library Manager or Git, ssilverman/QNEthernet).
   - SdFat (Teensy-compatible, from Teensyduino or Library Manager).
3. Copy all `.h` and `.cpp` files into the project.
4. Select board: **Teensy 4.1**.
5. Build and upload.

You must still:

- Fill in the correct IRQ names for NVIC priority setup in `setupInterruptPriorities()`.
- Define the actual command semantics (e.g., what `type = 0x01` means, file paths, SPI behavior).
