# PCSX2x6 imas

This fork adds IDOLM@STER support on top of [PCSX2x6](https://github.com/PS2Homebrew-arcade/pcsx2x6).

These imas changes are based on the behavior documented and implemented by [Play-imas](https://github.com/moonmagian/Play-imas/tree/imas):

- card reader communication through a Windows named pipe
- shutter status response for the IDOLM@STER cabinet check
- touchscreen input for IDOLM@STER

Currently, IDOLM@STER card reader support is intended for Windows builds using YaCardEmu.

## Running IDOLM@STER

### Step 1: Place arcade files

Put the dongle file in your PCSX2x6 memcards folder:

```text
Documents\PCSX2x6\memcards\[dongle file]
```

Put the HDD CHD in the `subdir` folder specified by the game's `.acgame` file.

For example, if the `.acgame` file contains `subdir=NM00022`, place the HDD CHD here:

```text
[acgame folder]\NM00022\*.chd
```

### Step 2: Start YaCardEmu

Use YaCardEmu for card reader emulation:

```text
https://github.com/GXTX/YACardEmu
```

Set this in the YaCardEmu config:

```ini
serialpath = \\.\pipe\imas
```

Start `YaCardEmu.exe` before starting the game.

For im@s image printing and more accurate erase behavior, use my modified fork:

```text
https://github.com/PeanutNeko/YACardEmu-imas
```

### Step 3: Enable touchscreen JVS mode

Add `jvsmode=touchscreen` to the game's `.acgame` file:

```ini
jvsmode=touchscreen
```

Then start `The iDOLM@STER`.

## Controls

JVS controls can be configured from the PCSX2x6 settings:

```text
Settings -> Controller -> JVS Controls
```

Control notes:

```text
SERVICE: Configure in JVS Controls
ENTER: Square
TEST: Configure in JVS Controls
```

The test menu can be used to set free play and disable closing time. Offline mode can be configured using Bandai Namco's offline mode instructions:

```text
https://www.idolmaster.jp/imas/arcade/idolmaster_offline.pdf
```

Touchscreen input uses the mouse position only while the left mouse button is pressed.

## How It Works

IDOLM@STER uses the System 246/256 AC UART path to talk to the card reader. This fork implements the imas card reader link with a Windows named pipe:

```text
\\.\pipe\imas
```

YaCardEmu connects to that pipe and emulates the reader side.

The game also checks cabinet shutter status through JVS outputs. After the game sends the GPIO shutter-check signal, this fork overrides the related JVS output sequence so the game sees the expected cabinet state.

The touchscreen path maps mouse clicks to the game's touch coordinates when `jvsmode=touchscreen` is enabled.

## Related Projects

- PCSX2x6: https://github.com/PS2Homebrew-arcade/pcsx2x6
- PCSX2: https://github.com/PCSX2/pcsx2
- Play-imas: https://github.com/moonmagian/Play-imas/tree/imas
- YaCardEmu: https://github.com/GXTX/YACardEmu
- YACardEmu-imas: https://github.com/moonmagian/YACardEmu-imas

---

<p align="center">
  <a href="https://ps2homebrew-arcade.github.io/pcsx2x6/">
    <img src="./bin/resources/icons/AppIconLarge.png" alt="Logo" width="25%" height="auto">
  </a>

  <p align="center">
    A fork of PCSX2 to emulate NAMCO System246 and System256 arcade units
    <br />
  </p>
</p>

## Get Started
> Please refer to [our website](https://ps2homebrew-arcade.github.io/pcsx2x6/) for that

## Special Thanks to:
- Tovarichtch, DiscoStarSlayer, Uyjulian, krHACKen, and many more for all your help
- Berion for the new app icon
