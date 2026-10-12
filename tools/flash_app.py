#!/usr/bin/env python3
"""Flash an existing single app slot, restart over USB and verify startup."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def single_app_slot(table_bytes, offset):
    """Accept factory or sole ota_0, never guess between multiple boot slots."""
    entries = []
    for index in range(0, len(table_bytes) - 31, 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from('<HBBII16sI', table_bytes, index)
        if magic == 0x50AA:
            entries.append((kind, subtype, start, size, flags))
    apps = [entry for entry in entries if entry[0] == 0]
    if (offset != 0x10000 or len(apps) != 1 or apps[0][1] not in (0, 0x10)
            or apps[0][2] != offset or apps[0][4] != 0):
        raise SystemExit('Expected exactly one unencrypted factory or ota_0 app at 0x10000')
    _, subtype, start, size, _ = apps[0]
    if size <= 0 or size % 4096:
        raise SystemExit('Invalid application partition size')
    if any(kind != 0 and data_start < start + size and start < data_start + data_size
           for kind, _, data_start, data_size, _ in entries):
        raise SystemExit('Application partition overlaps data')
    return start, size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--port', default='/dev/cu.usbmodem101')
    parser.add_argument('--previous-app', type=Path, help='Also verify this backup against the current app before writing')
    parser.add_argument('--log', type=Path, default=ROOT / 'build/flash-startup.log')
    parser.add_argument('--until', default='PLAYER_MENU,HOME', help='Startup log marker that confirms the new firmware is ready')
    parser.add_argument('--observe-seconds', type=float, default=0,
                        help='Keep the startup USB connection open to record the test (0..600 seconds)')
    parser.add_argument('--observe-until', help='End that observation at this log marker')
    args = parser.parse_args()
    if not math.isfinite(args.observe_seconds) or not 0 <= args.observe_seconds <= 600:
        parser.error('--observe-seconds must be between 0 and 600')
    build = args.build_dir.resolve()
    metadata = json.loads((build / 'flasher_args.json').read_text())
    if metadata['extra_esptool_args']['chip'] != 'esp32s3':
        raise SystemExit('Expected an ESP32-S3 build')
    app = build / metadata['app']['file']
    elf = app.with_suffix('.elf')
    table = build / metadata['partition-table']['file']
    offset = int(metadata['app']['offset'], 0)
    table_bytes = table.read_bytes()
    _, slot_size = single_app_slot(table_bytes, offset)
    image = app.read_bytes()
    if not image or (len(image) + 4095) // 4096 * 4096 > slot_size:
        raise SystemExit('App does not fit entirely inside the application partition')
    elf_sha = hashlib.sha256(elf.read_bytes()).hexdigest()[:8]
    command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3',
               '--port', args.port, '--baud', '460800']
    verify = [metadata['partition-table']['offset'], str(table)]
    if args.previous_app:
        verify += [hex(offset), str(args.previous_app.resolve())]
    # A mismatch aborts before writing. No bootloader/table/data erases or writes.
    try:
        subprocess.run(command + ['--after', 'no-reset', 'verify-flash'] + verify, check=True)
    except subprocess.CalledProcessError as error:
        raise SystemExit('Pre-flash verification failed; nothing was written. The board remains in bootloader. '
                         'Check the live partition layout/backup against this build before retrying.') from error
    subprocess.run(command + ['--before', 'no-reset', '--after', 'watchdog-reset',
                             'write-flash', hex(offset), str(app)], check=True)
    capture = [sys.executable, str(ROOT / 'tools/capture_serial.py'),
                    '--port', args.port, '--output', str(args.log), '--seconds', '35',
                    '--until', args.until, '--require-elf', elf_sha,
                    '--follow-seconds', str(args.observe_seconds)]
    if args.observe_until:
        capture += ['--follow-until', args.observe_until]
    subprocess.run(capture, check=True)
    log = args.log.read_text()
    if not any('ELF file SHA256:' in line and elf_sha in line for line in log.splitlines()):
        raise SystemExit('Ready marker received, but new firmware identity was not captured; startup is unverified')
    print(f'FLASH_READY: new app {elf_sha} booted and reached {args.until}; no power cycle needed')


if __name__ == '__main__':
    main()
