#!/usr/bin/env python3
"""Exercise the actual bounded file reader against valid and malformed files."""
import ctypes
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='sd-rom-test-') as tmp:
    directory = Path(tmp)
    library = directory / 'reader.so'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared',
                    '-fPIC', str(root / 'components/esp32-gameboy/src/storage/rom_file.c'), '-o', str(library)], check=True)
    reader = ctypes.CDLL(str(library)).rom_file_read
    reader.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    reader.restype = ctypes.c_int
    rom = (root / 'roms/big2small.gb').read_bytes()
    path = directory / 'rom.gb'
    guarded = (ctypes.c_uint8 * (len(rom) + 2))()
    buffer = ctypes.cast(ctypes.byref(guarded, 1), ctypes.POINTER(ctypes.c_uint8))
    for content, expected in [(b'', 2), (rom[:-1], 2), (rom + b'\x00', 2),
                               (rom * 4, 2), (rom, 0)]:
        ctypes.memset(guarded, 0xa5, len(guarded))
        path.write_bytes(content)
        assert reader(bytes(path), buffer, len(rom)) == expected
        assert guarded[0] == guarded[-1] == 0xa5
        assert path.read_bytes() == content
        if expected == 0:
            assert bytes(buffer[:len(rom)]) == rom
        else:
            assert bytes(buffer[:len(rom)]) == b'\xa5' * len(rom)
    path.unlink()
    assert reader(bytes(path), buffer, len(rom)) == 1
    assert not path.exists()
    assert reader(bytes(directory / 'absent/rom.gb'), buffer, len(rom)) == 1
    assert reader(bytes(directory), buffer, len(rom)) != 0
    assert reader(None, buffer, len(rom)) == 3
    assert reader(bytes(path), None, len(rom)) == 3
    assert reader(bytes(path), buffer, 0) == 3
print('PASS: exact ROM bytes, missing/empty/truncated/oversized files, buffer guards, no file changes')
