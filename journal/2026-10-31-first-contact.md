# 2026-10-31 · First contact

## Goal

Plug the board into my Mac and talk to it.

## What happened

- The board showed up as `/dev/cu.usbmodem21101`. USB reports it as Espressif's "USB JTAG_serial debug unit", so it uses the ESP32-S3's native USB, with no separate USB-to-serial chip.
- Installed `esptool` and `pyserial` in a Python venv. `esptool chip-id` identified an **ESP32-S3-PICO-1** with 8 MB flash and 8 MB PSRAM, which is how I know it's the **V2** board.
- Read the serial log. It was running an ESP-IDF build of Espressif's BLE GATT client example (`GATTC_DEMO`): it starts Wi-Fi, shuts it down, then scans for BLE devices. It's not Waveshare's factory firmware.
- Waveshare's wiki has no pin table, so I pulled the pins from the V2 examples in their repo. They're in [docs/board.md](../docs/board.md).

## Lessons

- V1 and V2 boards need different example code. Check the chip before flashing anything.
- Waveshare's factory firmware is in `03_Firmware` of their repo if I need to restore it.

## Next

- [ ] Draw something on the e-paper
- [ ] Read the SHTC3 temperature and humidity
- [ ] Try deep sleep with RTC wake-up
