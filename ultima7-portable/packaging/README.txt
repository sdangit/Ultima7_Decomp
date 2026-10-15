ULTIMA VII - PORTABLE
=====================

Two programs that run the Ultima VII games on a modern computer, without
DOSBox:

  Ultima7    Ultima VII: The Black Gate (with Forge of Virtue)
  Serpent    Ultima VII Part Two: Serpent Isle (with The Silver Seed)

They are only the programs: you need your own copy of each game you want to
play. Music plays as on an AdLib card, or on a Roland MT-32 if you add its ROM
files, which are not included.

The Serpent Isle port is new and has so far been tested on macOS only.


WHAT YOU NEED
-------------

1. The game files, from an installed copy (for example the GOG releases). The
   folder you want is the one that contains a folder named STATIC:
     Black Gate:     usually next to U7.EXE and ULTIMA7.COM
     Serpent Isle:   usually next to SI.EXE and SERPENT.COM

2. For MT-32 music and sound effects (optional): the Roland MT-32 ROM files,
   a "control" ROM and a "PCM" ROM. They must be from an MT-32; CM-32L ROMs
   are not used. The file names don't matter. Without them the games play
   their AdLib music and sound effects instead.


SETTING UP
----------

Each program goes with its own game: Ultima7 with The Black Gate, Serpent
with Serpent Isle. Set up only the ones you have.

Windows and Linux:

1. Copy the game folder (the one with STATIC in it) somewhere you can write
   to, such as Documents. The game saves into this folder. Don't use a folder
   inside Program Files.

2. Put the program into that folder, next to STATIC:
     Black Gate:     Ultima7.exe (Windows), Ultima7 (Linux)
     Serpent Isle:   Serpent.exe (Windows), Serpent (Linux)

   Or name the game folder Ultima7 (Black Gate) or Serpent (Serpent Isle) and
   put it in your Documents folder; the program then finds it from anywhere.

3. If you have them, put the two MT-32 ROM files into the same folder.

When you are done, the folders look like this (the games' other files can
stay where they are):

     Ultima7\
       STATIC\
       Ultima7.exe         (Ultima7 on Linux)
       MT32_CONTROL.ROM    (any name; optional)
       MT32_PCM.ROM        (any name; optional)
       U7.EXE, ULTIMA7.COM and other files from the game

     Serpent\
       STATIC\
       Serpent.exe         (Serpent on Linux)
       MT32_CONTROL.ROM    (any name; optional)
       MT32_PCM.ROM        (any name; optional)
       SI.EXE, SERPENT.COM and other files from the game

macOS (Apple Silicon Macs only):

1. Open the disk image and drag Ultima7 and Serpent onto the Applications
   folder.

2. Copy each game folder (the one with STATIC in it) into your Documents
   folder: name The Black Gate's Ultima7 and Serpent Isle's Serpent. The
   games save into these folders.

3. If you have them, put the two MT-32 ROM files into each of those folders.

When you are done, Documents looks like this:

     Documents/
       Ultima7/
         STATIC/
         MT32_CONTROL.ROM  (any name; optional)
         MT32_PCM.ROM      (any name; optional)
         U7.EXE, ULTIMA7.COM and other files from the game
       Serpent/
         STATIC/
         MT32_CONTROL.ROM  (any name; optional)
         MT32_PCM.ROM      (any name; optional)
         SI.EXE, SERPENT.COM and other files from the game

You don't need to change any settings: music plays on the MT-32 when the
ROMs are there and on the AdLib otherwise, and speech is always on.


PLAYING
-------

Windows:  Double-click Ultima7.exe or Serpent.exe. (Not ULTIMA7.COM, U7.EXE,
          SERPENT.COM or SI.EXE: those are the original DOS programs and
          won't run.) The first time, Windows may show "Windows protected
          your PC" because the programs are not signed: click "More info",
          then "Run anyway".

macOS:    Open Ultima7 or Serpent from Applications or Launchpad. The first
          time, macOS asks whether to open an app downloaded from the
          internet, then whether it may use your Documents folder: allow
          both.

Linux:    Double-click Ultima7 or Serpent, or run ./Ultima7 or ./Serpent
          from the folder. Needs a desktop with OpenGL and sound (ALSA), and
          a recent system (glibc 2.39 or newer, such as Ubuntu 24.04).

Each game starts with its introduction (press Esc to skip it), then the main
menu, as the original did. Start a new game there, or journey onward with
your saved game. To quit, close the window. Alt-Enter (Option-Return on a Mac
keyboard) switches between a window and full screen.

Your saved games are kept in each game's folder (the GAMEDAT folder and the
GAME*.U7 files). Back them up by copying them somewhere safe.


FOR ADVANCED USERS
------------------

Started from a terminal, each program accepts options (--help lists them):

  --data <folder>    use the game files in this folder
  --settings <file>  use a custom settings file
  --save-settings    save effective settings before launching
  --scale <N>        initial window size: N times 320x240 (default 3)
  --display <N>      macOS: open on display N (default: primary)
  --list-displays    macOS: list connected displays and sizes, then exit
  --scanlines[=N]    scanline darkness from 0 to 1 (bare flag uses 0.3)
  --music-device=M  auto, adlib, or roland
  --music-volume=N  combined music/SFX output volume, 0 to 100 (default 100)
  --speech-volume=N speech output volume, 0 to 100 (default 100)
  --cheat            enable the original cheat keys
  --adlib            AdLib music and effects, even when the MT-32 ROMs are there
  --roland           MT-32 music and effects (the default when the ROMs are there)
  --mt32-short-waits skip the original's pauses after MT-32 memory writes
  --no-quiet-weapons and --no-mt32-short-waits override saved true values

The scale is a positive whole number; --scale=4 also works. On macOS, window
sizes use points before Retina scaling. The scale is capped to a whole number
that fits the selected display, allowing room for the Dock, menu bar, and window
decorations. Other platforms use the existing screen-fit policy. You can still
resize the window or toggle fullscreen afterward.

On macOS, --list-displays works without game data. Use a number from that list
with --display=2 (or --display 2); these numbers may change when displays are
connected or disconnected. Explicitly requesting an unavailable display reports
an error. A disconnected saved monitor falls back to primary for that launch.

Scanlines are off by default. --scanlines turns them on at 30% darkness; use
--scanlines=0.15 for a subtler effect, --scanlines=0.5 for darker bands, or
--scanlines=0 to turn them off. This effect is available on all platforms.

Volume controls preserve the original synth settings and apply to the rendered
audio. Music and effects remain combined; speech is adjusted separately. Both
default to 100%, and 0 mutes output without stopping playback. Values above 100
are rejected. The in-game on/off preferences still apply. A space instead of =
also works, for example --music-volume 60.

SAVED SETTINGS

Each game reads settings.cfg from its game data folder. If missing, it creates a
commented default file on startup. Keys omitted from a file use internal defaults.
CLI arguments override the file for one launch. Add --save-settings to persist
them; otherwise even a first run writes defaults rather than CLI overrides.

The generated file documents each setting and its allowed values:

  scale = 3                # positive whole number (maximum 13421772)
  display = primary        # macOS: primary, display number, or uuid:UUID
  scanlines = 0            # darkness 0..1; 0=off
  music_volume = 100       # whole number 0..100; combined music/SFX
  speech_volume = 100      # whole number 0..100
  music_device = auto     # auto, adlib, roland
  quiet_weapons = false   # true, false
  mt32_short_waits = false # true, false

Use # or ; for full-line or inline comments. Keys and keywords are case-sensitive.
Malformed lines, unknown/duplicate keys, invalid values and file errors stop
launch; invalid files remain untouched, even with CLI overrides. Saving rewrites
the documented file. --settings <file> selects a different location; relative
paths use the current working directory and parent folders must already exist.
Help and --list-displays do not read or write settings.

On macOS, --save-settings converts a connected numbered display choice to a
stable UUID and saves the requested scale, before fitting it to a screen. Other
platforms retain but ignore the saved macOS display preference. In-game audio
on/off preferences stay in the original game files.

These environment variables are also available:

  U7_DATA            the game folder, instead of --data
  U7_MT32_ROMS       a folder holding the MT-32 ROMs, if not the game folder

Messages and errors are printed to the terminal.


LICENSES
--------

These programs include libmatoya (MIT license), Munt's mt32emu and Nuked
OPL3 (both GNU Lesser General Public License 2.1 or later). Their license texts
are in the licenses folder. Ultima VII and Serpent Isle are trademarks of Electronic Arts; the
game files are not included.
