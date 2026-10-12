# Menus and controls

Menus show a centered item list and selection, with no title, mode, help or
page text at the screen edges, apart from the small controller badge on Home
and Controller settings. Short lists use larger text. Settings has four
categories: Display, Controller, Sound and FPS. Both controller transports use
the same Connection selector and Map buttons wizard.

| Board input | Menu | Gameplay |
| --- | --- | --- |
| PWR tap | Next | Pause, save and return Home |
| BOOT tap | Select | Pause, save and return Home |
| PWR hold (700 ms) | Back | The initial press already pauses |
| BOOT hold (700 ms) | Previous | The initial press already pauses |
| BOOT + PWR together (2 s) | Toggle USB controller / serial | Pause/save, then toggle USB controller / serial |

| Controller input | In menus |
| --- | --- |
| Up / Down | Move selection |
| A / Start | Select |
| B | Back |
| Left / Right | Decrease / increase an editable value |

Values can be edited through Increase / Decrease using only the board buttons;
controller Left / Right still changes them directly. Back cancels editors,
confirmation screens and mapping. Affirmative confirmations remain explicit,
with Keep current selected first. Navigation Back/Cancel entries are not shown.

Mapping builds a candidate layout and saves it only when all eight buttons are
complete. Cancellation, disconnection or storage failure keeps the prior layout.
Prompts advance without a full-screen cleanup between buttons. Hold PWR to cancel.

## Saves and SD files

Home offers Games, Settings and Power off, plus Resume game when a game is loaded. Reopening a game with a valid checkpoint offers Continue saved / Start new. If the controller disconnects, the game pauses and returns Home.

- Cartridge battery saves (`.sav`) contain the game's own save data. They are written about once a minute while playing and whenever you pause.
- Pausing also replaces one automatic emulator checkpoint per game, preserving the exact execution position. Continue saved restores it. Exiting after losing overwrites the previous checkpoint too; manual slots and a rollback menu are not implemented.
- `.bak` files recover missing or unreadable saves; they are not selectable checkpoints. `.tmp` files are used during replacement. Save failures keep live progress for retry.

```text
/roms/game.gb                     ROM (up to 64 entries, displayed alphabetically)
/roms/game.sav                    cartridge battery save
/roms/game.gb.state               automatic resume checkpoint
/inkboy/presets/dithered/*.json    your dithered presets
/inkboy/presets/grayscale/*.json   your grayscale presets
```

For a `.gbc` file, the battery save is `game.gbc.sav`. Preset folders are created when you first save a preset. Use Home → Power off before removing the SD card; wait for `SAFE TO REMOVE SD CARD`. BOOT wakes the board.

## Controller status

Home and Settings → Controller show a badge. It updates only when the state changes; other menus have no badge.

| Badge | Meaning |
| --- | --- |
| Solid gamepad + BLE / USB | Connected |
| Crossed gamepad + BLE WAIT / USB WAIT | Waiting for the selected controller |
| BLE OFF / USB OFF | Controller service unavailable |
| PC icon + USB: PC | USB controller stopped; serial/flashing enabled |

For switching USB between controller and computer use, see [USB controller mode](flashing.md#usb-controller-mode).
