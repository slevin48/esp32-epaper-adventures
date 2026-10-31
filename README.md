# ESP32-S3 e-Paper Adventures

My build log for the **Waveshare ESP32-S3 1.54" e-Paper AIoT Development Board (V2)**: a 200×200 black/white e-paper screen, Wi-Fi and BLE, audio, sensors and a battery connector on one small board.

## The board

- **SoC:** ESP32-S3-PICO-1-N8R8, dual-core 240 MHz, 8 MB flash, 8 MB PSRAM
- **Display:** 1.54" e-paper, 200×200, black/white (SPI)
- **Onboard:** ES8311 audio codec, microphone and speaker, SHTC3 temperature/humidity sensor, PCF85063 RTC, TF card slot, battery connector, BOOT and PWR buttons, 2 × 6-pin expansion headers
- **USB:** native USB-Serial/JTAG (shows up as `/dev/cu.usbmodem*` on macOS)

Full pin map and setup notes: [docs/board.md](docs/board.md)

## Repo layout

| Folder | Contents |
|---|---|
| [`journal/`](journal/) | Dated entries: what I tried, what worked, what broke |
| [`projects/`](projects/) | Code for each experiment, one folder per project |
| [`docs/`](docs/) | Reference notes: pins, tooling, flashing |

## Journal

- 2026-10-31 · [First contact](journal/2026-10-31-first-contact.md): connecting to the board, identifying it, finding the pin map

## Resources

- [Waveshare wiki](https://docs.waveshare.com/ESP32-S3-ePaper-1.54)
- [Waveshare example code](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54) (use the **V2** examples; V1 code is not compatible)
- [Battery assembly guide (video)](https://www.youtube.com/watch?v=IWxYoRuI4wc)
- [ESP-IDF docs (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
