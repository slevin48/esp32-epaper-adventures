# 01 · Hello e-paper

Draws "Hello, world!" on the e-paper, blinks the LED (GPIO 3) every 500 ms and prints the chip info plus a tick counter over USB serial.

## Build and flash (Arduino CLI)

```bash
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32      # tested with 3.3.12
arduino-cli lib install GxEPD2            # tested with 1.6.9

FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi"
arduino-cli compile -b "$FQBN" -u -p /dev/cu.usbmodem21101 hello_epaper
arduino-cli monitor -p /dev/cu.usbmodem21101 -c baudrate=115200
```

## Expected serial output

```
hello_epaper: starting
chip: ESP32-S3 rev 2, flash 8 MB, PSRAM 8 MB
hello_epaper: display updated
tick 0
tick 1
```

## Gotchas

- **PSRAM must be `opi` (octal).** With `PSRAM=enabled` (quad) the boot log shows `quad_psram: PSRAM chip is not connected, or wrong PSRAM line mode` and the sketch sees 0 MB.
- **The display power switch (GPIO 6) is active low.** Drive it `LOW` before `display.init()`.
- The panel uses an SSD1681 controller, so GxEPD2's `GxEPD2_154_D67` class works. A full refresh takes about 1.8 s.
- `USBMode=hwcdc` and `CDCOnBoot=cdc` route `Serial` to the native USB port.
