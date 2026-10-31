# Board reference: Waveshare ESP32-S3-ePaper-1.54 V2

## Identifying V2

V2 has "V2" printed on the back or in the top-left corner of the PCB, and uses the **ESP32-S3-PICO-1-N8R8** (8 MB flash, 8 MB PSRAM). V1 used the ESP32-S3FH4R2 (4 MB flash, 2 MB PSRAM). Waveshare says example code is **not interchangeable** between versions.

`esptool chip-id` on my board reports:

```
Chip type:   ESP32-S3-PICO-1 (LGA56) (revision v0.2)
Features:    Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded Flash 8MB (XMC), Embedded PSRAM 8MB (AP_3v3)
USB mode:    USB-Serial/JTAG
```

## Pin map (V2)

Taken from Waveshare's V2 ESP-IDF examples (`02_Example/ESP-IDF/V2`), since the wiki doesn't publish a table.

| Function | GPIO |
|---|---|
| E-paper SPI | MOSI 13, SCK 12, CS 11, DC 10, RST 9, BUSY 8 |
| E-paper power enable | 6 |
| I2C (SHTC3, PCF85063, ES8311) | SDA 47, SCL 48 |
| RTC interrupt | 5 |
| I2S audio | MCLK 14, BCLK 15, WS 38, DOUT 45, DIN 16 |
| Speaker amp enable | 46 |
| Audio power | 42 |
| TF card (SDMMC 1-bit) | CLK 39, CMD 41, D0 40 |
| BOOT button | 0 |
| PWR button | 18 |
| Battery power hold | 17 |
| Battery voltage | ADC1 channel 3 (GPIO 4) |
| LED | 3 |
| Touch (Touch version only) | INT 21, RST 7 |

## Battery

The battery plugs into the 2-pin MX1.25 lithium battery connector. I followed Waveshare's video: [Battery assembly guide: ESP32 1.54-inch e-ink screen series](https://www.youtube.com/watch?v=IWxYoRuI4wc).

- Hold **PWR** (GPIO 18) to turn the board on or off when running on battery.
- Firmware keeps the board powered by driving **GPIO 17** (battery power hold) high.
- Battery voltage can be read on **ADC1 channel 3 (GPIO 4)**. See Waveshare's `01_ADC_Test` and `07_BATT_PWR_Test` examples.

## Tooling

- **ESP-IDF:** v5.5 or newer
- **Arduino:** arduino-esp32 v3.3.0 or newer; LVGL v8.3.11 or v9.3.0 and SensorLib v0.3.1
- **Flashing / serial:** `pip install esptool pyserial`

```bash
esptool --port /dev/cu.usbmodem21101 chip-id      # identify the board
idf.py -p /dev/cu.usbmodem21101 flash monitor      # build, flash, watch logs (ESP-IDF)
```

If flashing fails, hold **BOOT** while plugging in USB to force download mode.
