# Big2Small

[Big2Small](https://mdsteele.games/big2small/) is a puzzle game by **Matthew D. Steele** for Game Boy and Game Boy Color. InkBoy includes the unmodified official **v1.0.0** ROM. Copy `big2small.gb` into `/roms` on your FAT32 SD card, then open Games on the device.

## Licence and source

The game is **GPL-3.0-or-later**, separately from InkBoy. Its copyright and licence notices are retained in the source archive.

- [GNU GPL licence](big2small/LICENSE)
- [Complete matching source and build files](big2small/source-v1.0.0.tar.gz)
- [SHA-256 checksums](big2small/SHA256SUMS) (paths relative to the InkBoy repository root)
- [Upstream v1.0.0 release](https://github.com/mdsteele/big2small/releases/tag/v1.0.0)
- [Source commit](https://github.com/mdsteele/big2small/tree/ac7f03b2bee1a850a427443999db2057bc426854)

The source archive is an unmodified export of that release commit, including artwork, music, build tools, tests, Makefile and upstream README. Extract it and follow its build instructions: install RGBDS and run `make`; the output is `out/big2small.gb`.

Big2Small is a separate SD-card game, not embedded in the firmware. If redistributing the ROM, also provide its licence and matching source. InkBoy's MIT licence does not replace the game's GPL licence.
