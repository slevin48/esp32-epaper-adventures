# 03 · InkBoy: Game Boy on e-paper with an Xbox controller

Replicating Bo Made It's [GameBoy on E-Paper ESP32 at 30FPS](https://www.youtube.com/watch?v=XiZmM6hwwAU) on my board. It runs [kuslota/InkBoy](https://github.com/kuslota/InkBoy) (MIT), which ships a custom display driver for 30 fps 1-bit or 4-grey video, plus a Game Boy emulator, an SD card ROM browser and audio.

Upstream only supports the author's custom ESP32-C3 gamepad. [`inkboy-xbox-internal-flash.patch`](inkboy-xbox-internal-flash.patch) adds the **Xbox Wireless Controller** over Bluetooth LE, lets games run from internal flash when there's no SD card, and removes the black frame around the game.

> **Warning (from upstream):** custom waveforms and high refresh rates may cause permanent image retention or panel damage. Long-term panel life has not been validated.

## Xbox controller support

The patch changes `components/ble_gamepad/ble_gamepad.c`:

- Connects to any BLE HID device whose name starts with `Xbox`, as well as `ESP32 Gamepad`.
- Decodes Xbox input report 1 (firmware 5.x layout) into InkBoy's raw button bits, so the default mapping works without remapping:

  | Xbox | Game Boy |
  |---|---|
  | D-pad or left stick | D-pad |
  | A, Y | A |
  | B, X | B |
  | View | Select |
  | Menu | Start |

- Treats the Xbox pad as connected while the link is open. It only sends reports on change, and upstream drops a controller after 2 s without a report.

The controller needs Bluetooth LE: Xbox Series X|S, Elite Series 2, or Xbox One S (model 1708) with up-to-date firmware.

## No black frame

The patch makes **FILL SCREEN** the default game border, instead of BLACK LOGO. The 160×144 image is scaled 1.25× to 200×180 (every fourth row and column is doubled), leaving 10 px strips at the top and bottom. To get the original pixel-perfect image, choose **Settings → Display → Border → WHITE** on the board.

## Games without an SD card

The patch also renames the unused 4.9 MB `spiffs` partition to a FAT partition called `storage`. When the SD card fails to mount, InkBoy uses that partition instead, with the same `/roms` layout, saves and presets. Any `.gb` files in `InkBoy/internal_fs/roms/` are packed into it at build time and written by `idf.py flash`.

- Re-flashing overwrites the internal partition, including saves made on it.
- Only use ROMs you have the right to use, for example dumped from your own cartridges.

## Build and flash

```bash
git clone --recursive https://github.com/kuslota/InkBoy.git InkBoy   # tested at 823265e
cd InkBoy
git apply ../inkboy-xbox-internal-flash.patch

source ~/esp/esp-idf-v6.1/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem21101 flash monitor
```

Wake the board first (press BOOT) or hold BOOT while plugging in USB.

## Play

1. Format a microSD card as FAT32, create `/roms`, and copy `roms/big2small.gb` (free, GPL) or your own ROMs onto it. Insert it with the board powered off.
2. Put the Xbox controller in pairing mode: hold the pair button on top until the Xbox logo flashes fast.
3. On the board: **Settings → Controller → Connection → Bluetooth**.

## Back to Philolog

The Philogotchi v1 image is in `backups/philogotchi-v1/` (git tag `philogotchi-v1`).
