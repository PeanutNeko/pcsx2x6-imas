# Recovering settings from a real SYSTEM246

Some of you may eventually want to try this out, and it Is pretty straightforward!

## Requirements
- Jailbroken security dongle or SD2PSX that can run unsigned code from the machine to dump settings from
- USB Thumb drive
- [SRAM Dumping tool](https://github.com/PS2Homebrew-arcade/sram-dumper/releases)

## How to use
With your jailbroken SYSTEM246. run the dumper elf from the USB thumb drive, and the program will dump the on-board SRAM into a file on the same location


!!! note ""

    make sure to take notes of the game version that generated that SRAM Dump!
    Also, a valid Dump should be **exactly** 32kb (`32768` Bytes)

now that you have the file, just use it to replace the SRAM bin file that the emulator reads for your game.