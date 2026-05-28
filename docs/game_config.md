# game config

Since the execution of arcade games consists on the interaction with several files (a main executable, a disc/HDD image and a memory card image). the system2x6 games will be executed via a special per-game settings file

this files provide all the information the emulator needs to setup the enviroment and run that game

the file uses the INI format, and the extension must be `.acgame`

here's an example of how it should look like in the inside

```ini
[game]
name=SoulCalibur II (SC21 vA10 + DVD0D)
gameid=NM00007
[data]
elf=boot.elf 
dongle=NM00007 SC21, Ver.A10 a025781148773a.bin
card=ConquestCard0.bin
mediasrc=dvd0.img
media=DVD
```

## items Breakdown

> mandatory entries will have their name in bold

section | entry | value expected | description
------- | ----- | -------------- | ------------
game    | **name**  | string         | cosmetic game title for error messages or game list
game    | **gameid**| string         | Sony official gameID, used for artwork and automatic patches if we ever need such thing
data    | **platform** | string  | value must be `246` or `256`. this is used to indicate the emulator if the game is a SYSTEM256 specific game, and as such, the overclock emulation needs to be applied
data    | **elf**   | string         | elf filename to be executed _(must be at the same location than config file)_
data    | **dongle**| string         | security dongle filename _(must be at the same location than config file)_
data    | card  | string         | secondary memory card, only useful for SoulCalibur2 Conquest mode (and maybe to trigger certain dev-enviroment behavior on some games)
data    | **mediasrc**| string       | filename of the media file _(must be at the same location than config file)_
data    | **media** | string         | media type of the file from `mediasrc`, value must be `CD`, `DVD` or `HDD`
data    | 256Region | string     | system256 region override, used by emulator to fake regional signature, saves you from the hassle of having several NVRAM files for the same bios
data    | sram  | string  | filename for the SRAM settings. if not found, defaults to `sram.bin`
