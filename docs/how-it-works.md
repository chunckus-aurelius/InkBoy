# How InkBoy works

This page explains how an e-paper screen can show a Game Boy game at about 30 frames per second. The numbers come from the author's board and are measurements, not promises. For the settings themselves, see [tuning.md](tuning.md).

## Why e-paper is slow

An e-paper pixel holds charged black and white particles in a fluid. To change a pixel, the display applies a voltage for a while and the particles drift. Nothing happens when you write the new image into the controller's memory: the image RAM only tells the controller what to do. A waveform, a timed sequence of voltages, does the actual work, and the result depends on the old shade, the new shade, the voltage and how long it was applied.

The SSD1681 controller on this board runs one complete waveform per update, for every pixel that is part of it. A standard partial update takes about 360 ms of controller time, so you get two or three updates a second. Using it faster the obvious way also fails: pixels that did not change still receive part of the waveform, so the background slowly turns gray and moving objects leave trails.

## Per-pixel drive

[PaperBoy](https://gitlab.com/zephray/paperboy) showed that a different approach works. Its display software keeps a counter for every pixel. A pixel that gets a new target is driven toward it for a few passes, then left alone. Unchanged pixels are never driven.

PaperBoy scans the raw panel signals itself, so it controls each pixel's voltage on every pass. This board does not allow that. The panel is behind the SSD1681 controller, which only accepts image RAM and a waveform table. InkBoy borrows the idea and adapts it to what the controller offers.

The controller has two image planes. For each pixel, the pair of bits selects one of the waveform's rows, and InkBoy defines the rows as:

| Plane bits | Meaning |
| --- | --- |
| 00 | HOLD: no drive for this pixel |
| 10 | BLACK: drive toward black |
| 01 | WHITE: drive toward white |

Before every scan, the firmware computes a black mask and a white mask. A pixel that is not in either mask is held. A pixel in a mask is driven during that scan. Only the pixels that need drive are driven, and the rest of the screen stays still.

Three changes make a scan short enough to repeat many times a second.

- **A one-frame waveform.** The factory partial waveform is 17 controller frames long. InkBoy replaces it with a fixed waveform that has a single active frame, so one scan drives for roughly 20 ms in dithered mode and roughly 14 ms in grayscale mode.
- **Powering up once.** The panel's analog supply is started once and kept on between scans, which saves about 90 ms per update. It is switched off only when nothing is left to draw.
- **Faster transfers.** The two 5,000-byte planes are sent at 20 MHz, the maximum the controller datasheet allows for writes.

## Budgets and HOLD endings

In dithered mode, each pixel has a counter. When a pixel's target changes to black, its counter is set to the black pulse budget. When it changes to white, it gets the white pulse budget. Each scan that is delivered to the panel subtracts one from the pixels it drove. At zero the pixel is held. A new target restarts that pixel's count, so a pixel that reverses direction in the middle of a budget starts over without disturbing its neighbors.

Counters only change after a scan was really delivered. A failed transfer does not use up any budget.

When the last drive pixel finishes, InkBoy sends one more scan with every pixel in HOLD. This is the HOLD ending: it lets the waveform finish cleanly instead of stopping in the middle of a drive. If new motion arrives first, the ending is dropped and the new image goes out with its own masks.

A bigger budget gives better contrast and cleaner ghost removal, but a moving object stays in an in-between state longer. The tuning guide covers the trade-off.

## Pacing at 30 fps

The emulator runs on its own at the Game Boy's 59.7 frames per second, on its own core. The display does not follow every frame. Each time a slot comes up, it takes the latest finished image from a three-buffer handoff and plans that. Old frames are never queued.

Slots are 33.334 ms apart and are anchored to a fixed schedule rather than to the previous scan. A late scan does not shift every later slot, and a missed slot is skipped, not repeated. In dithered mode one scan fits in each slot. In grayscale mode two scans fit in a slot, which pair mode uses (see below). A new image takes priority over a completion scan that would not fit in the slot, so the picture stays current even when drive is left over.

The result is about 30 updates per second. That counts images handed to the panel, and it does not mean every pixel has finished settling in 33 ms. Heavy scenes can take longer than a slot to plan and then drop slightly below 30.

## Dithered mode

The Game Boy has four shades and the dithered mode has two. It converts shades to patterns with a fixed 4×4 Bayer matrix: white has no black dots, the two grays are about 5/16 and 11/16 black, and black is solid. The pattern is tied to screen positions and never changes from frame to frame, so a still image stays still. Each source pixel stays one panel pixel, so the game fits the 200×200 panel with a margin.

## Grayscale mode: partial drive doses

Grayscale mode does not dither. It stops pixels partway between black and white. A pixel moves toward its target in small steps called doses, one dose per scan, and the number of doses decides where it ends up. Four shades are labeled K (black), D (dark gray), L (light gray) and W (white).

### Twelve transitions

Every pixel goes from one shade to another, giving 4×4 combinations. The four with the same shade need no drive, which leaves 12 different transitions. Each one has its own dose count, and the counts are different because the particles do not respond the same in every direction. Black to white and white to black are both long drives, but a pixel moving from black toward gray responds gradually, while one moving from white toward gray reacts more abruptly. Reaching L from K, from D and from W are three different jobs.

The Default table (from shade, to shade) is:

| From \ to | K | D | L | W |
| --- | --- | --- | --- | --- |
| K | - | 2 | 4 | 7 |
| D | 4 | - | 1 | 6 |
| L | 4 | 1 | - | 5 |
| W | 7 | 3 | 2 | - |

### Pair mode and HOLD

Two scans fit in a slot, so each slot can send a drive scan and a follow-up scan right behind it. In the follow-up, a transition can either continue driving or be held. A transition whose HOLD flag is on stops right after its drive scan, which ends the pulse promptly and suits the gray targets. A transition with HOLD off gets a second dose in the same slot, which suits the black and white endpoints that need more drive. Each of the 12 transitions has its own flag.

### Retargeting

The firmware keeps a model of every pixel: its estimated level, its target and how many doses remain. The model is a bookkeeping estimate and not a measurement of reflectance. When the game changes a pixel's target before the previous one finished, the firmware uses the level the pixel actually reached to work out the new dose. The retarget setting scales how much of the original dose is used.

### History and reversal compensation

Particles remember. A pixel that is light gray, goes to black and returns to light gray ends up slightly different from one that was never driven. InkBoy records the last settled shade of each pixel and the drive it has received since, for one excursion. Two cases are corrected:

- **A completed return.** The pixel got all the way to the other shade and is coming back. Its return needs a little less or more drive. The Default removes one dose from light gray to black to light gray.
- **A reversal.** The pixel turned around before the first drive finished. The return dose is scaled from how far the pixel had gone. The Default uses 150% for light gray to black to light gray.

Six round-trip routes are tunable. Two more options weaken the voltage of the black-directed pulses for a pixel moving from settled white to light gray, and weaken the voltage that lightens a pixel.

## The two-core planner

Planning a scan means walking all 40,000 panel pixels and deciding what each one needs. In grayscale mode that is too much work for one core to do in a slot. The per-pixel state is in external RAM, where reading and writing it pixel by pixel is slow. So the planner copies a small working set to internal RAM and splits the rows of the screen into two halves, one for the display task and one for a worker on the other core. The two halves are then joined.

If internal RAM is too short for the worker, the planner falls back to doing everything on one core. The results are identical and tests compare both. The only difference is speed.

## What the safety check guards

The display controller has commands that burn waveforms into one-time memory. Those commands can permanently alter the controller. The driver deliberately excludes them, and the checker guards that reviewed command surface. `tools/check_safety.py` fails the build if any of this changes:

- The driver sends only a reviewed list of commands. Command 0x30, which programs waveform memory, is not in the list and does not appear in the source.
- All waveforms go through the volatile LUT command 0x32, from one fixed loader. There is no general call that takes a LUT, and no call that takes a raw voltage.
- Only the driver talks to the display over SPI. Nothing in the firmware calls an eFuse function.
- The grayscale voltage choices are fixed presets, and the weaker drive for settled white to light gray is limited to the two values the menu offers.
- Secure boot and flash encryption stay off in the default configuration, so a build cannot lock a development board.

The check reviews source and configuration; it is not a proof against runtime faults or hardware damage. It does not validate custom waveforms, permanent image retention, or long-term panel life. Both fast modes remain experimental and are used at your own risk, including their default presets.
