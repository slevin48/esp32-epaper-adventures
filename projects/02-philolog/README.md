# 02 · Philolog

A daily dose of philosophy on the e-paper. It shows one French nugget a day, taken from my [philolog](https://github.com/slevin48/philolog) knowledge base (`notes/nuggets.fr.md`, 30 days, from Socrates to Sisyphus).

## How it behaves

- **Every day at 08:00 (Paris)** the board wakes, draws the day's **quote and author**, and goes back to deep sleep. The e-paper keeps the quote on screen with no power.
- **Press BOOT or PWR** (the side buttons) to see more. Each press shows the next page: title and explanation, then *À essayer*, then the source. A page counter (`2/3`) sits in the corner.
- After the last page, or **2 minutes** without a press, it goes back to the quote.
- **Hold PWR for 3 s** to switch off when running on battery.
- Jour 1 was 2026-10-31. After Jour 30 the cycle starts again.

Page turns use partial refresh while the board is awake (20 s after each press). The first draw after waking is a full refresh.

## Time

The PCF85063 RTC keeps the time, stored in UTC. The firmware converts it to Paris time, with daylight saving. If the RTC has lost its time (for example after the battery was fully disconnected), the firmware:

1. syncs over Wi-Fi (NTP) if `philolog/secrets.h` exists:
   ```c
   #define WIFI_SSID "my-network"
   #define WIFI_PASS "my-password"
   ```
2. otherwise falls back to the time the firmware was built.

You can also set the clock over USB while the board is awake by sending `T<unix epoch>` on the serial port.

## Build

The nuggets come from the private philolog repo, so the generated `nuggets.h` is not committed. Generate it first:

```bash
python3 tools/gen_nuggets.py ~/path/to/philolog/notes/nuggets.fr.md

arduino-cli lib install GxEPD2 U8g2_for_Adafruit_GFX
FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi"
arduino-cli compile -b "$FQBN" -u -p /dev/cu.usbmodem21101 philolog

# right after flashing, while the board is awake:
python3 -c "import time; print('T%d' % time.time())" > /dev/cu.usbmodem21101
```

The generator turns `œ`, `—` and `…` into `oe`, `-` and `...`, because the u8g2 fonts only cover Latin-1. It also inserts non-breaking spaces before `? ! : ; »` and after `«`, so French punctuation never starts a line.

## Gotchas

- **The I2C bus is powered by the audio rail (GPIO 42, active low).** With it off, the RTC doesn't answer at all (`short read`). Waveshare's examples turn it on before any I2C traffic.
- **The RTC can hold junk from other firmware.** Mine came back from the factory firmware reading 1 January 2056 with the OS (oscillator stop) flag clear. The firmware rejects any RTC time earlier than its build date or more than 20 years after it.
- Send `R` over serial while the board is awake to see the RTC registers as read at boot.

- In deep sleep the USB port disappears from the Mac. Press a button to wake the board before flashing, or hold BOOT while plugging in.
- The Arduino preprocessor puts function prototypes above struct definitions, so `Line` lives in `line.h`.
- GPIO 6 (e-paper power) and GPIO 17 (battery hold) are latched with `gpio_hold_en` during deep sleep. Otherwise the board switches itself off on battery.
