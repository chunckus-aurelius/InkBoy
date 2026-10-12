# InkBoy

InkBoy runs Game Boy games on a Waveshare ESP32-S3 e-paper board at about 30 fps. Choose fast dithered black and white or four-shade grayscale. It includes an SD ROM browser, speaker audio, automatic resume, tunable display presets and an optional FPS counter.

[![Watch the video](docs/thumbnail.png)](https://www.youtube.com/watch?v=XiZmM6hwwAU)

## Planned, not implemented yet

- Support for generic Bluetooth HID gamepads, beyond the custom InkBoy controller.
- Better USB connectivity handling, to make switching between controller use and flashing/serial easier.

## Hardware

| Part | Requirement |
| --- | --- |
| [Waveshare ESP32-S3-ePaper-1.54 V2](https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54) | 200×200 e-paper screen, speaker and microSD slot |
| microSD card | FAT32, for ROMs, saves and presets |
| [InkBoy controller](https://github.com/kuslota/inkboy-controller) | Custom ESP32-C3 gamepad, connected over BLE or a USB-C data cable |

**Only the custom InkBoy controller is currently supported.** Generic Bluetooth and USB HID gamepads are not supported.

## Experimental display drive

> [!WARNING]
> **Use at your own risk.** Custom waveforms and high refresh rates may cause permanent image retention or panel damage. Long-term panel life has not been validated. This applies to default presets, source builds and prebuilt firmware; software checks do not guarantee panel safety.

## Quick start

### 1. Build and flash

Install ESP-IDF v6.1 with [Espressif Installation Manager](https://docs.espressif.com/projects/idf-im-ui/en/latest/index.html) and open its **IDF Terminal**. Navigate to where you want the project saved (no spaces in the path), connect the board with a USB-C data cable, then run:

```sh
git clone --recursive https://github.com/kuslota/InkBoy.git
cd InkBoy
idf.py set-target esp32s3
idf.py build
idf.py flash
```

This creates an `InkBoy` folder in your current directory. See the [flashing guide](docs/flashing.md) for folder setup, subsequent builds, prebuilt firmware and USB recovery.

### 2. Add games

Format the SD card as FAT32 and copy your ROMs into `/roms`. Power the board off before inserting or removing the card. The [Big2Small puzzle game](roms/README.md) is included: copy `roms/big2small.gb` onto the card. It has its own GPL licence; no proprietary game ROMs are bundled.

### 3. Connect the controller

Choose **Settings → Controller → Connection**:

- **Bluetooth:** turn on the custom controller. InkBoy finds it automatically as `ESP32 Gamepad`.
- **USB:** connect the controller with a USB-C to USB-C **data** cable and select USB.

If your button wiring differs, use **Settings → Controller → Map buttons**.

## Controls and saves

| Input | In menus |
| --- | --- |
| Controller Up / Down | Move selection |
| Controller A / Start; B | Select; back |
| PWR tap; hold 0.7 s | Next item; back |
| BOOT tap; hold 0.7 s | Select; previous item |

During gameplay, the controller provides the usual Game Boy buttons. Press **BOOT or PWR** to pause, save and return Home.

**Each pause replaces the game's single resume checkpoint.** Continue saved restores it; manual save slots and rollback are not available. Use **Home → Power off** before removing the SD card. BOOT wakes the board.

The controller badge on Home and in Controller settings shows connected, WAIT (disconnected), OFF (service unavailable), or **USB: PC** (serial/flashing enabled). Full controls, badge meanings and save files are in the [controls guide](docs/ui-controls.md).

### USB controller mode

The controller shares the port with flashing and serial. Unplugging it does **not** free the port automatically.

**To flash:** hold **BOOT + PWR together for at least 2 seconds** until the badge shows **USB: PC**, disconnect the controller, connect your computer, then flash normally. The saved USB preference is unchanged. Restarting returns to controller mode; to return without restarting, disconnect the computer and repeat the gesture.

**For repeated flashing, logging or automation, select Bluetooth first.** USB then stays free across restarts. Some serial tools reset the board when opening the port. See [USB switching and recovery](docs/flashing.md#usb-controller-mode) for details.

## Compatibility

Game Boy (DMG) games and dual-mode cartridges in DMG mode, up to 2 MiB. Color-only games and RTC cartridges are not supported. Busy scenes may drop below 30 fps; grayscale can leave faint trails. See [full limitations](docs/compatibility.md).

## More details

- [Display settings and presets](docs/tuning.md)
- [How the display works](docs/how-it-works.md)
- [Development checks and releases](docs/development.md)

## Contributing

Found display settings that work better? Open a pull request with your preset or changes to pulse counts, HOLD, history corrections or shade transitions. Include your board/display model, what you tested, and what improved or got worse; comparison photos or videos are welcome. See the [tuning guide](docs/tuning.md) for the available settings.

## Credits

- [PaperBoy](https://gitlab.com/zephray/paperboy) by Wenting Zhang (Zephray) inspired this project and the per-pixel driving approach. No PaperBoy code is included in InkBoy; see [components/esp32-gameboy/THIRD_PARTY_NOTICES.md](components/esp32-gameboy/THIRD_PARTY_NOTICES.md) for how the emulator core was obtained.
- The emulator core comes from [CrankBoy](https://github.com/CrankBoyHQ/crankboy-app) and [Peanut-GB](https://github.com/deltabeard/Peanut-GB), through the `esp32-gameboy` component. Sound uses the MiniGB APU from Peanut-GB.
- The controller enclosure is based on a design by SBPS, with attribution in the controller repository.
- The ES8311 driver is from Espressif's esp_codec_dev. The display's fixed waveform tables come from Waveshare's [e-Paper library](https://github.com/waveshareteam/e-Paper).

Full license information is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

MIT, see [LICENSE](LICENSE). Third-party components keep their own licenses.
