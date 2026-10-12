#!/usr/bin/env python3
"""Capture after flashing without explicitly requesting a reset; tolerate USB reconnects."""
import argparse
import math
import os
import signal
import time
from datetime import datetime, timezone
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='/dev/cu.usbmodem101')
parser.add_argument('--output', default='results/baseline.log')
parser.add_argument('--seconds', type=float, default=90)
parser.add_argument('--record', action='store_true',
                    help='Continuously record until SIGINT/SIGTERM, including reconnects and errors; no startup verification')
parser.add_argument('--until', default='PLAYER_MENU,HOME',
                    help='Stop at this log marker (default: the Home menu drawing)')
parser.add_argument('--require-elf', help='Require this ELF hash prefix before the ready marker')
parser.add_argument('--follow-seconds', type=float, default=0,
                    help='After ready, keep this same USB connection open for bounded observation (max 600 s)')
parser.add_argument('--follow-until', help='End observation early at this additional marker')
args = parser.parse_args()
if not math.isfinite(args.follow_seconds) or not 0 <= args.follow_seconds <= 600:
    parser.error('--follow-seconds must be between 0 and 600')
if args.record and (args.require_elf or args.follow_seconds or args.follow_until):
    parser.error('--record cannot be combined with startup/follow verification')
Path(args.output).parent.mkdir(parents=True, exist_ok=True)
deadline = time.monotonic() + args.seconds
port = None
connected = False
ready = False
identity_seen = args.require_elf is None
stopping = False
def stop_recording(signum, frame):
    global stopping
    stopping = True
if args.record:
    signal.signal(signal.SIGINT, stop_recording)
    signal.signal(signal.SIGTERM, stop_recording)
with open(args.output, 'a' if args.record else 'w') as output:
    def capture_event(event):
        line = f'CAPTURE_{event},utc,{datetime.now(timezone.utc).isoformat()},pid,{os.getpid()}'
        print(line, flush=True)
        output.write(line + '\n'); output.flush()
    if args.record:
        capture_event('RECORDING')
    pending = b''
    try:
        while not stopping and (args.record or time.monotonic() < deadline):
            try:
                if port is None:
                    port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
                    port.dtr = False
                    port.rts = False
                    port.port = args.port
                    port.open()
                    connected = True
                    if args.record:
                        capture_event('CONNECTED')
                pending += port.read(port.in_waiting or 1)
            except (serial.SerialException, OSError):
                if port is not None:
                    port.close()
                    port = None
                    if args.record and connected:
                        capture_event('DISCONNECTED')
                    connected = False
                pending = b''
                time.sleep(0.1)
                continue
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                line = raw.decode('utf-8', errors='replace').rstrip('\r')
                print(line, flush=True)
                output.write(line + '\n')
                output.flush()
                if args.record:
                    continue
                if 'ELF file SHA256:' in line and args.require_elf and args.require_elf in line:
                    identity_seen = True
                if any(marker in line for marker in (
                        'Stopped on error:', 'refusing automatic start', 'VENDOR_ERROR')):
                    raise SystemExit(1)
                if ready and args.follow_until and args.follow_until in line:
                    raise SystemExit(0)
                if not ready and args.until in line:
                    if not identity_seen:
                        raise SystemExit('Ready marker without required new ELF identity; startup unverified')
                    ready = True
                    print(f'CAPTURE_READY: reached {args.until}; required ELF={args.require_elf or "not requested"}', flush=True)
                    if not args.follow_seconds:
                        raise SystemExit(0)
                    deadline = time.monotonic() + args.follow_seconds
    finally:
        if port is not None:
            port.close()
    if args.record:
        capture_event('STOPPED')
    elif ready:
        print('CAPTURE_FOLLOW_TIMEOUT: startup verified; observation window ended', flush=True)
    else:
        raise SystemExit('Capture timeout: requested log marker was not observed')
