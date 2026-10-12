# Presets

`dithered/default.json` and `grayscale/default.json` are exactly what the firmware writes for its built-in Defaults. Copy one, change its `"name"`, and put it on the SD card under `/inkboy/presets/<mode>/` to make your own preset.

The format, every field and how to use presets on the device are in [docs/tuning.md](../docs/tuning.md#presets).

Regenerate these files with `python3 tools/export_default_presets.py`. A test fails if they drift from the firmware.
