# Ultima VII Part Two: Serpent Isle, portable

A port of the reconstructed Ultima VII Part Two: Serpent Isle source (originally 16-bit Borland
C++ for DOS) to modern systems, built the same way as the Black Gate port beside it. The game
code talks to the machine only through a small platform layer; the first backend uses
[libmatoya](https://github.com/snowcone-ltd/libmatoya) for the window, input, video, audio and
files, [Munt](https://github.com/munt/munt) to emulate the Roland MT-32 and
[Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) to emulate the AdLib, for music and sound
effects.

The macOS app builds, and its startup options and settings have been tested. Linux and Windows
requirements follow the Black Gate port; Serpent Isle has not been tested on those systems.

Everything needed to build is in this folder and the shared folders beside it. You supply the
game data and, optionally, the MT-32 ROMs.

## Layout

```text
ultima7-portable/Serpent Isle/
  CMakeLists.txt         the build
  cmake/Libmatoya.cmake  builds libmatoya with its own makefiles
  game/                  the game: .c files are C, .cpp files are C++, as originally written
  game/serpent/          the launcher (SERPENT.COM)
  game/mainmenu/, intro/, endgame/, shared/   the other programs, and what they share
  game/usecode/          the usecode source (reference, not compiled)
  platform/plat.h        everything the game needs from the host
  platform/matoya/       the libmatoya backend (window, input, audio, files, timing)
  tools/si_new_game.py   sets up a new game with a named Avatar
```

Shared with the Black Gate port, one level up in `ultima7-portable/`:

```text
  third_party/libmatoya/ libmatoya, vendored at a pinned commit (MIT)
  third_party/munt/      Munt's mt32emu library, vendored at a pinned tag (LGPL 2.1+)
  third_party/nuked-opl3/ Nuked OPL3, vendored at a pinned commit (LGPL 2.1+)
  packaging/             the players' README (both games), the macOS Info.plist, notarizing
  assets/icons/          program icons
```

## Requirements

As the Black Gate port (see its README), not yet tried on Linux or Windows:

| | macOS | Linux x64 | Windows x64 |
| --- | --- | --- | --- |
| Compiler | Xcode command line tools (clang) | GCC 11+ or clang 14+ | clang-cl, from Visual Studio 2022 with "C++ Clang tools for Windows" |
| Build tools | CMake 3.20+, make | CMake 3.20+, make | CMake 3.20+, Ninja and nmake (both come with Visual Studio) |
| Extra | | `glslangValidator` (Ubuntu/Debian: `glslang-tools`), for libmatoya's shaders | |

## Build

```sh
cmake -S "ultima7-portable/Serpent Isle" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target serpent
```

On Windows, from an "x64 Native Tools Command Prompt for VS 2022":

```bat
cmake -S "ultima7-portable/Serpent Isle" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build --target serpent
```

The first build also builds libmatoya (through its own makefile, into
`../third_party/libmatoya/bin/`) and mt32emu (as a CMake subproject). The program is
`build/Serpent` (`build\Serpent.exe` on Windows, the app `build/Serpent.app` on macOS).

Options:

- `-DU7_MT32_ROM_DIR=/path/to/roms` bakes in a default ROM folder (see below).
- `-DCMAKE_BUILD_TYPE=Debug` for debugging.
- `-DU7_RESET_CHECK=ON` (clang, macOS or Linux; for testing only) reports, after each handoff
  between programs, any game global that differs from how the first program found it.

## What you need to supply

**Game data.** A folder holding an installed copy of Serpent Isle, with its `STATIC` folder
(and `GAMEDAT`, if you have a game in progress). File names are matched in any letter case. The
game writes to this folder, so use a copy.

**MT-32 ROMs.** Munt needs the Roland MT-32 control and PCM ROM images (any file names; they
are recognised by content). The game looks in the `U7_MT32_ROMS` folder, else the
`U7_MT32_ROM_DIR` build option, else the game data folder. Without them music and effects play
on the AdLib.

**AdLib.** An OPL3 (Nuked OPL3) at any port, filtered as a Sound Blaster 16's FM output; its
timers run on the program clock, so the drivers detect it as on DOS. `--adlib` picks it even
when the ROMs are found; `--roland` picks the MT-32 (with no ROMs it falls back to the AdLib).

**Sound setup.** `SERPENT.CFG` is the setup the DOS installer wrote: line 1 the music card and
port (`s 388` as shipped), line 2 the speech card's port, IRQ, DMA and `o` for the Sound
Blaster Pro driver (`220 7 1 o`). The game reads line 1 as Roland (`r 330`) when the MT-32
plays and AdLib (`a 388`) otherwise, and with no full speech line it reads `220 7 1 o`, whatever the file says
(`platform/matoya/files.c`). The file on disk is never changed. The in-game audio options
still turn each part off.

## Run

```sh
build/Serpent --data /path/to/serpent
```

- `--data` is the game data folder (or set `U7_DATA`). By default it is the current folder if
  that has `STATIC`, else the folder the program is in (on macOS, the folder holding
  `Serpent.app`), else `Documents/Serpent`. The game checks for its core files at launch and
  names any that are missing or empty.
- This is the launcher, as `SERPENT.COM` was: it runs the main menu, intro, game and endgame in
  turn, in one process and one window, each starting afresh as its own EXE did. Other arguments
  go on to the game, with `p` added.
- `--program <name> <args>` runs one program alone: `si p` for the game (`p` is the flag the
  launcher passes; the game will not start without it), `mainmenu v`, `intro hisss`,
  `endgame hisss`.
- `--help` lists every switch.

The game's own command-line options, as switches (with what SI.EXE took):

| Switch | Original | Effect |
| --- | --- | --- |
| `--cheat` | `manimal` | cheat keys |
| `--cheat-start` | `s` | with `--cheat`: move at once, move anything, an Avatar that can't die, debug output |
| `--speech` | `v` | speech on |
| `--adlib[=port]` | `a` | AdLib music and effects, even when the MT-32 ROMs are found |
| `--roland[=n]` | `r` | Roland MT-32 music and effects (the default when the ROMs are found) |
| `--shape-pool=KB` | `c` | size of the shape cache |
| `--overlay-size` | `b` | report the DOS overlay buffer size, then stop |
| `--version` | `?` | legacy game version switch; currently does not print a port version or exit immediately from the launcher |

### Port command-line options

| Switch | Settings-file key | Values and behavior |
| --- | --- | --- |
| `--settings=FILE` | — | use this file instead of `settings.cfg` in the game data folder; relative paths use the current working directory |
| `--save-settings` | — | write effective settings, including CLI overrides, then launch; without this flag, CLI changes are temporary |
| `--scale=N` | `scale` | whole number 1..13421772, default 3; initial window size is `N` times 320x240, reduced to fit the display |
| `--display=N` | `display` | macOS only: a number from `--list-displays`, `primary` (default), or `uuid:UUID` |
| `--list-displays` | — | macOS only: list display numbers, UUIDs, desktop and usable sizes in points, and Retina backing scale, then exit; no game data required |
| `--scanlines[=N]` | `scanlines` | darkness 0..1, default 0 (off); bare `--scanlines` selects 0.3; 1 produces black bands |
| `--music-device=M` | `music_device` | `auto` (default), `adlib`, or `roland`; `auto` uses MT-32 when ROMs are found, otherwise AdLib; `--adlib` and `--roland` set the same preference |
| `--music-volume=N` | `music_volume` | whole-number percentage 0..100, default 100; music and sound effects share this output gain; 0 mutes, 100 preserves the original level |
| `--speech-volume=N` | `speech_volume` | whole-number percentage 0..100, default 100; separate speech output gain; 0 mutes, 100 preserves the original level |
| `--quiet-weapons` | `quiet_weapons` | set to `true`: no crackle from the fire sword and firedoom staff; default `false` |
| `--no-quiet-weapons` | `quiet_weapons` | set to `false`, overriding a saved `true` |
| `--mt32-short-waits` | `mt32_short_waits` | set to `true`: skip MT-32 memory-write settle pauses during timbre uploads at startup and for sound effects; default `false` |
| `--no-mt32-short-waits` | `mt32_short_waits` | set to `false`, overriding a saved `true` |

`--settings`, `--scale`, `--display`, `--music-device`, `--music-volume` and
`--speech-volume` accept either `--option=value` or `--option value`. For a scanline value,
use `--scanlines=N`; `--scanlines N` is not supported. If the same preference is supplied more
than once on the command line, the last value wins, including device aliases and boolean pairs.
Display selection and listing are macOS-only; the other settings work on all platforms.

For example, try these settings for one launch:

```sh
build/Serpent --data /path/to/serpent --scale=4 --scanlines=0.3 \
    --music-device=adlib --music-volume=100 --speech-volume=70
```

Add `--save-settings` to keep them for future launches. To use a separate preferences file:

```sh
build/Serpent --data /path/to/serpent --settings ./preferences.cfg \
    --scale=4 --scanlines=0.3 --save-settings
```

On macOS, replace `build/Serpent` in these examples with
`build/Serpent.app/Contents/MacOS/Serpent`.

`--scale=3` requests a 960x720 window; `--scale=4` requests 1280x960. On macOS these
are points, with Retina backing pixels handled automatically. On macOS the initial scale is
capped to the largest whole-number scale that fits the selected display's usable desktop area,
including room for the title bar and borders. Other platforms retain the existing screen-fit
policy (including its 90% height limit).
The option sets the initial window size; resizing and fullscreen still recalculate the rendering
scale automatically, preserving the 4:3 aspect ratio and nearest-neighbor filtering.

On macOS, run `Serpent.app/Contents/MacOS/Serpent --list-displays`, then use
`--display=2 --scale=4` to open on the second listed display. Display numbers are one-based
and describe the currently connected displays; check the list again after changing the display
setup. An unavailable explicitly requested display exits with an error. A disconnected saved
display falls back to primary for that launch. Sizing uses macOS's usable desktop bounds for the
Dock and menu bar; no additional Stage Manager margin is assumed.

`--scanlines` enables libmatoya's basic scanline effect at 30% darkness. Use
`--scanlines=0.15` for a subtler effect, `--scanlines=0.5` for darker bands, or
`--scanlines=0` to turn it off. It applies to the displayed image across the menu, intro,
game and ending, and requires no extra shader files. Very small displayed images skip the
effect. Screenshots from `U7_TEST_INPUT` capture the original framebuffer before GPU effects.

Volume controls scale the rendered audio without changing the synth's instrument settings,
MIDI volumes, fades, or music/SFX balance. `--music-volume=60` lowers music and effects together
to 60% of their existing sample amplitude, while `--speech-volume=80` sets speech to 80%.
Both default to 100 (unchanged); 0 mutes the output while playback timing continues. Values
above 100 are rejected. The controls apply on all platforms and persist across programs.
The in-game on/off preferences still apply. `U7_AUDIO_DUMP` includes the volume adjustments.

### Saved settings

Each game reads `settings.cfg` from its resolved game data folder.
The first successful startup creates a commented file containing internal defaults if it is
missing. Precedence is **CLI options > settings file > internal defaults**. Omitted keys use
internal defaults. The original DOS sound configuration does not select these port preferences.
CLI overrides apply only to the current launch unless `--save-settings` is supplied. This also
applies on the first run: without `--save-settings`, the new file contains defaults, not the CLI
experiment. Help and display listing do not read or write settings or require game data.

The file uses `key = value`, with full-line or inline `#` / `;` comments. Keys and keyword values
are case-sensitive. Its supported settings and defaults are:

```ini
scale = 3                 # whole number 1..13421772; requested size, before screen fitting
display = primary         # macOS: primary, a listed display number, or uuid:UUID
scanlines = 0             # 0..1 darkness; 0=off
music_volume = 100        # whole number 0..100; combined music/SFX output
speech_volume = 100       # whole number 0..100
music_device = auto       # auto, adlib, roland
quiet_weapons = false     # true, false
mt32_short_waits = false  # true, false
```

Use `--settings /path/to/preferences.cfg` (or `--settings=/path/to/preferences.cfg`) for a custom
file. Relative paths are relative to the current working directory. Missing custom files are
created too; their parent folder must already exist. For example:

```sh
Serpent.app/Contents/MacOS/Serpent --scale=6 --scanlines=0.3 --music-volume=80 --save-settings
```

Saving rewrites the file with documented effective settings. On macOS, a connected numbered
display choice is saved as a stable display UUID, and the requested scale is saved even when
its initial window is capped to fit. A disconnected saved UUID is retained. Non-macOS builds
retain but ignore the saved macOS display preference; explicit display CLI options remain
unsupported there.

Invalid CLI values, malformed settings lines, unknown or duplicate keys, out-of-range values
and file read/write errors stop launch with a specific error; invalid values do not silently fall
back to defaults. Settings-file errors identify the file and line where applicable. Invalid
existing files are never rewritten, even if a valid CLI override supplies that setting.
In-game music/speech/effects on/off preferences remain in the
original game files; `U7.CFG` / `SERPENT.CFG` are not modified by this feature.

To run the shared settings regression tests without game data:

```sh
cmake --build build --target u7_settings_test
ctest --test-dir build --output-on-failure
```

**Starting a new game.** Use the main menu's new game choice. For scripted runs that skip the
menu, this sets one up the way the menu does, and the game builds it on its next start:

```sh
python3 "ultima7-portable/Serpent Isle/tools/si_new_game.py" /path/to/serpent --name Jamie --female --skin 1
```

`--skin` is 0 (fair, the default), 1 or 2 (darkest), as the menu's six portraits. It copies the
current `GAMEDAT` next to the data folder first, because the game deletes it.

Other environment variables:

| Variable | Use |
| --- | --- |
| `U7_MT32_ROMS` | folder holding the MT-32 ROMs |
| `U7_AUDIO_DUMP` | write the mixed audio to this file (raw 16-bit stereo, 48 kHz) |
| `U7_TEST_INPUT` | scripted input for testing (see `platform/matoya/testinput.c`); the window stays hidden and no sound plays |

## Dependencies

As the Black Gate port: the libraries are vendored as plain source at fixed versions (see
`../third_party/README.md`). Munt is under the GNU Lesser General Public License 2.1 or later:
if you distribute a binary, you must also make it possible to relink it against a modified
mt32emu, and include `../third_party/munt/mt32emu/COPYING.LESSER.txt`. Nuked OPL3 is under the
same licence (`../third_party/nuked-opl3/LICENSE`).

## Known limitations

- **Null-pointer reads** that the DOS game relied on are served by a fault handler that exists
  only for arm64 macOS, x64 Windows and x64 Linux so far (`platform/matoya/nullpage.c`).
  Elsewhere they crash. Each one found is meant to be fixed in the game code.
