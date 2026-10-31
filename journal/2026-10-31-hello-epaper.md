# 2026-10-31 · Hello e-paper: flash, test, restore

## Goal

Do a full round trip: back up the flash, flash my own firmware, check it works, then put Waveshare's factory firmware back.

## 1. Back up

```bash
esptool --port /dev/cu.usbmodem21101 read-flash 0 ALL backups/original-flash-2026-10-31.bin
```

This read all 8 MB in 26 s. The backup holds the BLE scanner demo the board shipped with. It stays out of git (`backups/` is ignored) because the NVS partition can hold Wi-Fi credentials.

## 2. Hello world

I used Arduino CLI with arduino-esp32 3.3.12 and GxEPD2 1.6.9, which is much lighter than a full ESP-IDF install. Code and build steps: [projects/01-hello-epaper](../projects/01-hello-epaper/).

Serial output:

```
hello_epaper: starting
chip: ESP32-S3 rev 2, flash 8 MB, PSRAM 8 MB
hello_epaper: display updated
tick 0
```

## 3. Restore the factory firmware

Waveshare ships `03_Firmware/V2-FactoryProgram.bin` in their repo as one merged image, so it flashes at address 0:

```bash
esptool --port /dev/cu.usbmodem21101 erase-flash
esptool --port /dev/cu.usbmodem21101 write-flash 0 V2-FactoryProgram.bin
```

It booted as ESP-IDF v5.5.3 with the ES8311 codec, SHTC3 sensor, e-paper and LVGL all starting. The only error was the SD card mount, because there was no card in the slot.

## Lessons

- **PSRAM is octal.** The first build used quad PSRAM and the log said `quad_psram: PSRAM chip is not connected, or wrong PSRAM line mode`. Building with `PSRAM=opi` fixed it.
- **The e-paper power switch (GPIO 6) is active low.** Without driving it LOW the panel never powers up.
- The panel is an SSD1681, so GxEPD2's `GxEPD2_154_D67` driver works out of the box.
- Erasing the whole chip before restoring means no leftovers from the Arduino sketch confuse the factory app.

## Next

- [ ] Read the SHTC3 sensor and show live temperature and humidity on the e-paper
- [ ] Try partial refresh so updates don't flash the whole screen
- [ ] Deep sleep with RTC wake-up
