# Ultima VII: The Black Gate, portable

A port of the reconstructed Ultima VII: The Black Gate source (originally 16-bit Borland C++ for
DOS) to modern systems. The game code talks to the machine only through a small platform layer;
the first backend uses [libmatoya](https://github.com/snowcone-ltd/libmatoya) for the window,
input, video, audio and files, [Munt](https://github.com/munt/munt) to emulate the Roland
MT-32 and [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) to emulate the AdLib, for music
and sound effects.

Everything needed to build is in this folder and the shared folders beside it. You supply the game data and, optionally, the MT-32 ROMs.

## Layout

```text
ultima7-portable/Black Gate/
  CMakeLists.txt         the build
  cmake/Libmatoya.cmake  builds libmatoya with its own makefiles
  game/                  the game: .c files are C, .cpp files are C++, as originally written
  game/ultima7/          the launcher (ULTIMA7.COM)
  game/mainmenu/, intro/, endgame/, shared/   the other programs, and what menu and intro share
  platform/plat.h        everything the game needs from the host
  platform/matoya/       the libmatoya backend (window, input, audio, files, timing)
  tools/u7_new_game.py   sets up a new game with a named Avatar
```

Shared with the Serpent Isle port, one level up in `ultima7-portable/`:

```text
  third_party/libmatoya/ libmatoya, vendored at a pinned commit (MIT)
  third_party/munt/      Munt's mt32emu library, vendored at a pinned tag (LGPL 2.1+)
  third_party/nuked-opl3/ Nuked OPL3, vendored at a pinned commit (LGPL 2.1+)
  packaging/             the players' README (both games), the macOS Info.plist, notarizing
  assets/icons/          program icons
```

## Requirements

| | macOS (tested) | Linux x64 (tested under WSL2, Ubuntu 24.04) | Windows x64 (tested) |
| --- | --- | --- | --- |
| Compiler | Xcode command line tools (clang) | GCC 11+ or clang 14+ | clang-cl, from Visual Studio 2022 with "C++ Clang tools for Windows" |
| Build tools | CMake 3.20+, make | CMake 3.20+, make | CMake 3.20+, Ninja and nmake (both come with Visual Studio) |
| Extra | | `glslangValidator` (Ubuntu/Debian: `glslang-tools`), for libmatoya's shaders | |

On Linux, libmatoya opens X11, OpenGL/Vulkan and ALSA at run time, so a desktop session with
those libraries installed is needed to play. Under WSL, WSLg provides the window, but its sound
server is PulseAudio only: install `libasound2-plugins` and route ALSA to it with a `~/.asoundrc`
holding `pcm.!default { type pulse }` and `ctl.!default { type pulse }`, or the game starts with
"no audio output".

## Build

```sh
cmake -S "ultima7-portable/Black Gate" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target u7
```

On Windows, build from an "x64 Native Tools Command Prompt for VS 2022", with clang-cl (MSVC's
own `cl` rejects some of the game's C++):

```bat
cmake -S "ultima7-portable/Black Gate" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build --target u7
```

libmatoya's makefile relies on `cmd`'s `mkdir`. If Git's `usr\bin` is on `PATH`, nmake runs
Git's `mkdir` instead and the libmatoya step fails, so build from a prompt without it.

The first build also builds libmatoya (through its own makefile, into
`../third_party/libmatoya/bin/`) and mt32emu (as a CMake subproject). The program is `build/Ultima7`
(`build\Ultima7.exe` on Windows, the app `build/Ultima7.app` on macOS).

Options:

- `-DU7_MT32_ROM_DIR=/path/to/roms` bakes in a default ROM folder (see below).
- `-DCMAKE_BUILD_TYPE=Debug` for debugging. A sanitizer build is useful for finding memory bugs:
  `-DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize=alignment"` (same for
  `CMAKE_CXX_FLAGS`), then run with
  `ASAN_OPTIONS=handle_segv=0:handle_sigbus=0:detect_leaks=0`.
- `-DU7_RESET_CHECK=ON` (clang, macOS or Linux; for testing only) reports, after each handoff
  between programs, any game global that differs from how the first program found it.

## What you need to supply

**Game data.** A folder holding an installed copy of Ultima VII: The Black Gate, with its
`STATIC` folder (and `GAMEDAT`, if you have a game in progress). File names are matched in any
letter case. The game writes to this folder, so use a copy.

**MT-32 ROMs.** Munt needs the Roland MT-32 control and PCM ROM images (any file names; they
are recognised by content). CM-32L images and split control-ROM halves are not used. They are
not included. The game looks in the `U7_MT32_ROMS` folder, else the `U7_MT32_ROM_DIR` build
option, else the game data folder. Without them music and effects play on the AdLib.

**AdLib.** An OPL3 (Nuked OPL3) at any port, filtered as a Sound Blaster 16's FM output; its
timers run on the program clock, so the drivers detect it as on DOS. `--adlib` picks it even
when the ROMs are found; `--roland` picks the MT-32 (with no ROMs it falls back to the AdLib).

**Sound setup.** `U7.CFG` is the setup the DOS installer wrote. The game reads its music line
as Roland (`r 330`) when the MT-32 plays and AdLib (`a 388`) otherwise, and with no speech line it reads one (`220 7 1`), whatever the file says (`platform/matoya/files.c`). The
file on disk is never changed. The in-game audio options still turn each part off.

## Run

```sh
build/Ultima7 --data /path/to/ultima7
```

On macOS the program inside the app takes the same options:
`build/Ultima7.app/Contents/MacOS/Ultima7 --data /path/to/ultima7`.

- `--data` is the game data folder (or set `U7_DATA`). By default it is the current folder if
  that has `STATIC`, else the folder the program is in (on macOS, the folder holding
  `Ultima7.app`), else `Documents/Ultima7`, so a double-clicked `Ultima7` placed in the game
  folder finds its data on every platform. The game checks for its core files at
  launch and names any that are missing or empty.
- On Windows the program is a windowed one, so no console opens with it. Started from a
  terminal, it prints its messages there; redirected output goes to the file or pipe as usual.
- This is the launcher, as `ULTIMA7.COM` was: it runs the main menu, intro, game and endgame in
  turn, in one process and one window, each starting afresh as its own EXE did. Other arguments
  go on to the game.
- `--program <name> <args>` runs one program alone: `u7 -p` for the game (`-p` is the flag the
  launcher passes; the game will not start without it), `mainmenu v`, `intro ereiamjh`,
  `endgame ereiamjh`.
- `--help` lists every switch.
- Alt-Enter (Option-Return on a Mac keyboard) toggles fullscreen.

The game's own command-line options, as switches (with the letter U7.EXE took):

| Switch | Original | Effect |
| --- | --- | --- |
| `--cheat` | `ABCD` + Alt-255 | cheat keys (F1 lists them, on the game screen as in DOS) |
| `--cheat-start` | `s` | with `--cheat`: move at once, move anything, an Avatar that can't die, debug output |
| `--speech` | `v` | speech on |
| `--adlib[=port]` | `a` | AdLib music and effects, even when the MT-32 ROMs are found |
| `--roland[=n]` | `r` | Roland MT-32 music and effects (the default when the ROMs are found) |
| `--shape-pool=KB` | `c` | size of the shape cache |
| `--overlay-size` | `b` | report the DOS overlay buffer size, then stop |
| `--version` | `?` | show the version, then stop |

Settings of this port, not in the original:

| Switch | Effect |
| --- | --- |
| `--display=N` | macOS: display number from `--list-displays`, `primary`, or `uuid:UUID`; also accepts `--display N` |
| `--list-displays` | macOS: list connected displays, desktop and usable sizes in points, and Retina backing scale; no game data required |
| `--scale=N` | initial window size, `N` times 320x240; positive integer, default 3; also accepts `--scale N` |
| `--scanlines[=N]` | scanline darkness from 0 (off) to 1 (black bands); bare `--scanlines` uses 0.3; off by default; all platforms |
| `--music-volume=N` | combined music and sound-effect output volume, 0..100%, default 100; also accepts `--music-volume N` |
| `--speech-volume=N` | speech output volume, 0..100%, default 100; also accepts `--speech-volume N` |
| `--settings=FILE` | use this settings file instead of the game folder's `settings.cfg`; also accepts `--settings FILE` |
| `--save-settings` | save effective settings, including CLI overrides, before launching |
| `--music-device=M` | `auto`, `adlib`, or `roland`; overrides the saved device preference |
| `--no-quiet-weapons` | override a saved `quiet_weapons=true` |
| `--no-mt32-short-waits` | override a saved `mt32_short_waits=true` |
| `--quiet-weapons` | no crackle from the fire sword and firedoom staff, no hum from the magic sword, Hoe of Destruction, Magebane and Death Scythe |
| `--mt32-short-waits` | no pauses after MT-32 memory writes in the ending's music; the emulated MT-32 needs no settle time |

`--scale=3` requests a 960x720 window; `--scale=4` requests 1280x960. On macOS these
are points, with Retina backing pixels handled automatically. On macOS the initial scale is
capped to the largest whole-number scale that fits the selected display's usable desktop area,
including room for the title bar and borders. Other platforms retain the existing screen-fit
policy (including its 90% height limit).
The option sets the initial window size; resizing and fullscreen still recalculate the rendering
scale automatically, preserving the 4:3 aspect ratio and nearest-neighbor filtering.

On macOS, run `Ultima7.app/Contents/MacOS/Ultima7 --list-displays`, then use
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

**Saved settings.** Each game reads `settings.cfg` from its resolved game data folder.
The first successful startup creates a commented file containing internal defaults if it is
missing. Omitted settings use defaults, then explicitly supplied CLI options override the file.
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
music_device = auto      # auto, adlib, roland
quiet_weapons = false    # true, false
mt32_short_waits = false  # true, false
```

Use `--settings /path/to/preferences.cfg` (or `--settings=/path/to/preferences.cfg`) for a custom
file. Relative paths are relative to the current working directory. Missing custom files are
created too; their parent folder must already exist. For example:

```sh
Ultima7.app/Contents/MacOS/Ultima7 --scale=6 --scanlines=0.3 --music-volume=80 --save-settings
```

Saving rewrites the file with documented effective settings. On macOS, a connected numbered
display choice is saved as a stable display UUID, and the requested scale is saved even when
its initial window is capped to fit. A disconnected saved UUID is retained. Non-macOS builds
retain but ignore the saved macOS display preference; explicit display CLI options remain
unsupported there.

Malformed lines, unknown or duplicate keys, out-of-range values and file read/write errors stop
launch with a specific error. Invalid existing files are never rewritten, even if a valid CLI
override supplies that setting. In-game music/speech/effects on/off preferences remain in the
original game files; `U7.CFG` / `SERPENT.CFG` are not modified by this feature.

To run the shared settings regression tests without game data:

```sh
cmake --build build --target u7_settings_test
ctest --test-dir build --output-on-failure
```

**Starting a new game.** Use "Start New Game" in the main menu. For scripted runs that skip the
menu, this sets one up the way the menu does, and the game builds it on its next start:

```sh
python3 "ultima7-portable/Black Gate/tools/u7_new_game.py" /path/to/ultima7 --name Jamie --female
```

It copies the current `GAMEDAT` next to the data folder first, because the game deletes it.

Other environment variables:

| Variable | Use |
| --- | --- |
| `U7_MT32_ROMS` | folder holding the MT-32 ROMs |
| `U7_AUDIO_DUMP` | write the mixed audio to this file (raw 16-bit stereo, 48 kHz) |
| `U7_TEST_INPUT` | scripted input for testing, e.g. `"4000 key i; 6000 shot frame.ppm; 7000 quit"` (see `platform/matoya/testinput.c`); the window stays hidden and no sound plays |

## Dependencies

The libraries are vendored as plain source at fixed versions (see `../third_party/README.md`), so
no network access or git submodules are needed.

**libmatoya** has no CMake project of its own. `cmake/Libmatoya.cmake` runs its own makefile
(`GNUmakefile` on macOS and Linux, `makefile` with nmake on Windows) and links the resulting
static library, adding the system frameworks or libraries it needs. To build it by hand:

```sh
cd ultima7-portable/third_party/libmatoya
make -f GNUmakefile TARGET=macosx ARCH=arm64    # or TARGET=linux ARCH=x86_64 / aarch64
```

**mt32emu** is built from `../third_party/munt/mt32emu` as a CMake subproject, as a static
library with its C interface. The ROMs are loaded at run time, never linked in. Munt is under the
GNU Lesser General Public License 2.1 or later: if you distribute a binary, you must also make it
possible to relink it against a modified mt32emu (for example, by offering the game's object
files or building mt32emu as a shared library with `-Dlibmt32emu_SHARED=ON`), and include
`../third_party/munt/mt32emu/COPYING.LESSER.txt`.

**Nuked OPL3** (`../third_party/nuked-opl3/opl3.c`) is compiled into the program. It is under the
same licence as Munt, with the same obligations; its text is `../third_party/nuked-opl3/LICENSE`.

## Known limitations

- **Null-pointer reads** that the DOS game relied on are served by a fault handler that exists
  only for arm64 macOS, x64 Windows and x64 Linux so far (`platform/matoya/nullpage.c`).
  Elsewhere (arm64 Linux or Windows, for one) they crash.
- **The main menu, intro and endgame** are separate programs in the original and are not
  ported. Quitting ends the program; restart and endgame are not handled yet.
- **Game speed** follows the community frame-limiter patch: about 10 world updates a second.
