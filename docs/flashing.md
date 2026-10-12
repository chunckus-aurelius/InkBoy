# Flashing and USB access

## Build from source

Install ESP-IDF v6.1 using [Espressif Installation Manager](https://docs.espressif.com/projects/idf-im-ui/en/latest/index.html). In its dashboard, select v6.1 and choose **Open IDF Terminal** (or use the **IDF_PowerShell** shortcut on Windows). In that terminal, run `idf.py --version` and check that it reports v6.1.

Choose a project folder with no spaces in its full path. For example, on **macOS/Linux**:

```sh
mkdir -p "$HOME/esp"
cd "$HOME/esp"
```

Or on **Windows (PowerShell)**:

```powershell
New-Item -ItemType Directory -Force -Path C:\esp | Out-Null
cd C:\esp
```

Download InkBoy into that folder:

```sh
git clone --recursive https://github.com/kuslota/InkBoy.git
cd InkBoy
```

This creates `~/esp/InkBoy` on macOS/Linux, or `C:\esp\InkBoy` on Windows. **Already downloaded it?** Skip the clone step, navigate to your existing InkBoy folder in the activated terminal, and run `git submodule update --init --recursive`.

Connect the board with a USB-C data cable. From inside the InkBoy folder, run:

```sh
idf.py set-target esp32s3  # First build only
idf.py build
idf.py flash
```

For later builds, open an IDF terminal, navigate to your existing InkBoy folder, then run `idf.py build` and `idf.py flash`. Already using a USB controller? [Free the port first](#usb-controller-mode).

## Prebuilt firmware (optional)

When a release provides binaries, download `inkboy-<version>.bin` from the [releases page](https://github.com/kuslota/InkBoy/releases). Verify it against that release’s `SHA256SUMS`. It is a merged image: bootloader, partition table and app in one file, written at address 0.

```sh
esptool --chip esp32s3 write-flash 0x0 inkboy-<version>.bin
```

With older esptool versions the command is `esptool.py --chip esp32s3 write_flash 0x0 inkboy-<version>.bin`. You can also flash from a browser with [ESP Web Flasher (esptool-js)](https://espressif.github.io/esptool-js/): connect, set the offset to `0x0`, choose the file, and program.

If the board is not detected, follow [Recovering the USB port](#recovering-the-usb-port) below.

The merged image also covers the settings area of the flash, so flashing resets saved settings (display mode, active preset, controller choice, button mapping). Presets and saves on the SD card are not touched.

To upgrade and keep your settings, flash only the app image, `inkboy-<version>-app.bin`, from the same release page:

```sh
esptool --chip esp32s3 write-flash 0x10000 inkboy-<version>-app.bin
```

This leaves the bootloader, partition table and settings as they are, so use the merged image instead if a release says its partition layout changed.

## USB controller mode

USB controller mode keeps the shared port reserved, even with no controller attached.

1. Hold **BOOT + PWR together for at least 2 seconds**, then release. A running game pauses and saves; the badge changes to **USB: PC**.
2. Disconnect the controller, connect your computer, and flash or open serial.

Serial stays enabled until you repeat the gesture or restart. The saved USB preference is unchanged. To switch back without restarting, disconnect the computer, hold both buttons again, then connect the controller. The gesture has no effect when Bluetooth is selected.

**For repeated flashing, logging or automation, select Settings → Controller → Connection → Bluetooth.** USB then stays available across restarts. Some serial tools reset the board when opening the port; with USB selected, that restart restores controller mode.

## Recovering the USB port

If the menu responds, use the [USB toggle](#usb-controller-mode) above. Holding PWR during an actual startup also keeps serial enabled for that boot without changing the saved preference.

For ROM download mode, hold BOOT while performing a real reset or power cycle, then release BOOT. Use the board’s reset control if available; otherwise remove all power sources, including the battery, before restoring power with BOOT held. Simply unplugging and reconnecting USB does not reset a battery-powered board.
