#!/usr/bin/env python3
"""The shipped example presets are exactly what the firmware encodes for its compiled Defaults."""
import tempfile
from pathlib import Path

from export_default_presets import export
from host_build import ROOT

with tempfile.TemporaryDirectory(prefix='presets-') as directory:
    export(directory)
    for mode in ('dithered', 'grayscale'):
        generated = (Path(directory) / mode / 'default.json').read_bytes()
        shipped = (ROOT / 'presets' / mode / 'default.json').read_bytes()
        assert generated == shipped, f'presets/{mode}/default.json is stale; run tools/export_default_presets.py'
assert sorted(p.name for p in (ROOT / 'presets').iterdir() if p.name != '.DS_Store') == ['README.md', 'dithered', 'grayscale']
print('PASS: presets/dithered/default.json and presets/grayscale/default.json match the firmware encoder output for the Defaults')
