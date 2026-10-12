# Compatibility and limitations

- Game Boy (DMG) games only, plus dual-mode cartridges played in DMG mode. Color-only cartridges are rejected.
- Supported cartridge types: ROM only, MBC1, MBC2, MBC3 without a real-time clock, MBC5 without rumble. Cartridges with a clock (RTC) do not run.
- ROMs from 32 KiB to 2 MiB.
- Only the InkBoy controller is supported. Other Bluetooth or USB gamepads are not.
- Heavy scenes can drop slightly below 30 fps, mostly in grayscale mode.
- Grayscale mode still leaves faint trails after some shade changes. Pausing and restarting gameplay establishes a fresh display reference.
- Custom display driving is experimental; see [Experimental display drive](../README.md#experimental-display-drive). The software checks do not establish long-term panel safety.
