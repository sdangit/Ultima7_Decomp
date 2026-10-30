ULTIMA VII - MODERN
===================

One program, Ultima7Modern, that plays both Ultima VII games on a modern
computer, without DOSBox:

  Ultima VII: The Black Gate (with Forge of Virtue)
  Ultima VII Part Two: Serpent Isle (with The Silver Seed)

Unlike the portable programs it is not held to the originals' exact behaviour:
it brings Serpent Isle's paperdolls and other conveniences to The Black Gate,
adds wide screens and a start menu with settings, and keeps its saved games in
its own folder. Core gameplay stays as the originals had it.

It is only the program: you need your own copy of BOTH games. The Roland
MT-32 ROM files are optional and not included.

Ultima VII Modern is new and has so far been tested on macOS only.


WHAT YOU NEED
-------------

1. Both games' files, from installed copies (for example the GOG releases).
   For each game, the folder you want is the one that contains a folder named
   STATIC:
     Black Gate:     usually next to U7.EXE and ULTIMA7.COM
     Serpent Isle:   usually next to SI.EXE and SERPENT.COM

2. For MT-32 music and sound effects (optional): the Roland MT-32 ROM files,
   a "control" ROM and a "PCM" ROM. They must be from an MT-32; CM-32L ROMs
   are not used. The file names don't matter.


SETTING UP
----------

The program only reads the game folders; it never writes to them. There are
two ways to set it up:

  - Game folders in Documents: saved games and settings go to your user data
    folder (see SAVED GAMES below).

  - Everything in one folder: put both game folders next to the program.
    Saved games and settings are then kept in that folder too, so the whole
    set-up can be moved or backed up as one folder. It must be a folder you
    can write to, such as one in Documents, not one inside Program Files or
    Applications.

Windows and Linux:

1. Put the Ultima7Modern folder from this archive wherever you like.

2. Copy each game folder (the one with STATIC in it) into your Documents
   folder: name The Black Gate's Ultima7 and Serpent Isle's Serpent.

   Or, for everything in one folder, put the two game folders, named the
   same way, into the Ultima7Modern folder next to the program:

     Ultima7Modern\
       Ultima7Modern.exe   (Ultima7Modern on Linux)
       Ultima7\            (The Black Gate, with STATIC in it)
       Serpent\            (Serpent Isle, with STATIC in it)
       settings.json and Saves\ appear here once you play

3. If you have them, put the two MT-32 ROM files into either game folder.

When you are done, Documents looks like this:

     Documents\
       Ultima7\
         STATIC\
         MT32_CONTROL.ROM  (any name; optional)
         MT32_PCM.ROM      (any name; optional)
         U7.EXE, ULTIMA7.COM and other files from the game
       Serpent\
         STATIC\
         SI.EXE, SERPENT.COM and other files from the game

macOS (Apple Silicon Macs only):

1. Open the disk image and drag Ultima7Modern onto the Applications folder.

2. Copy each game folder (the one with STATIC in it) into your Documents
   folder: name The Black Gate's Ultima7 and Serpent Isle's Serpent, as
   shown above.

   Or, for everything in one folder, put Ultima7Modern and the two game
   folders together in a folder of your own in Documents, instead of
   dragging the app into Applications.

3. If you have them, put the two MT-32 ROM files into either game folder.

If your Documents folder is kept in iCloud Drive with "Optimize Mac Storage"
on, macOS may remove the game files from the Mac and fetch them again when
the game reads them, which pauses the game. To avoid this, Control-click each
game folder in Finder and choose "Keep Downloaded".


PLAYING
-------

Windows:  Double-click Ultima7Modern.exe. The first time, Windows may show
          "Windows protected your PC" because the program is not signed:
          click "More info", then "Run anyway".

macOS:    Open Ultima7Modern from Applications or Launchpad. The first time,
          macOS asks whether to open an app downloaded from the internet,
          then whether it may use your Documents folder: allow both.

Linux:    Double-click Ultima7Modern, or run ./Ultima7Modern from its folder.
          Needs a desktop with OpenGL and sound (ALSA), and a recent system
          (glibc 2.39 or newer, such as Ubuntu 24.04).

The program starts with a menu: pick The Black Gate or Serpent Isle, or open
Settings. Each game then starts with its introduction (press Esc to skip it)
and its own main menu, as the original did. Leaving a game returns to the
start menu. Alt-Enter (Option-Return on a Mac keyboard) switches between a
window and full screen.

If a game folder is missing or incomplete, the program says which one and
stops.


SETTINGS
--------

Settings in the start menu:

  Screen shape     4:3, 16:10 or 16:9; the wider shapes show more of the world
  Scaling          Fill, or Pixel-exact (whole-number scaling only)
  Full screen      Off or On
  Sound            Best available, Adlib, MT32 or OPL3:
                     Best available  MT-32 when its ROMs are found, else OPL3
                     Adlib           the original FM music and effects
                     MT32            the Roland MT-32 (only offered with ROMs)
                     OPL3            FM with stereo and more voices
  Volume           0% to 100%, for music, effects and speech
  Townsfolk pace   Original, or Relaxed (townsfolk at half speed)
  Runic writing    Original runes, or Translated
  Cheats           Off or On (the original cheat keys)


SAVED GAMES
-----------

Saved games and settings are kept next to the program when both game
folders are there too. Otherwise they are kept in the user data folder:

  Windows:   %APPDATA%\Ultima7Modern
  macOS:     ~/Library/Application Support/Ultima7Modern
  Linux:     ~/.local/share/ultima7modern (or $XDG_DATA_HOME/ultima7modern)

The program's --help shows which folder it is using.

Each game has its own folder under Saves. Saved games of the original games
or the portable programs are not imported. Back up the Saves folder by copying
it somewhere safe.


FOR ADVANCED USERS
------------------

Started from a terminal, the program accepts options (--help lists them all):

  --game bg|si       play that game without the start menu
  --bg-data <folder> The Black Gate's game folder
  --si-data <folder> Serpent Isle's game folder
  --saves <folder>   where saved games go
  --cheat            enable the original cheat keys
  --adlib, --opl3, --roland
                     choose the music and effects for this run only

and these environment variables:

  U7_USER_DATA       the folder for saves and settings, instead of either above
  U7_MT32_ROMS       a folder holding the MT-32 ROMs, if not a game folder

The MT-32 ROMs may also go in the user data folder. Messages and errors are
printed to the terminal.


LICENSES
--------

This program includes libmatoya (MIT license), Munt's mt32emu and Nuked OPL3
(both GNU Lesser General Public License 2.1 or later). Their license texts are
in the licenses folder. Most of The Black Gate's paperdoll pictures are Grant
W. Chapman's art (github.com/GrantWChapman/Ultima-VII-Artwork), used with his
permission. Ultima VII and Serpent Isle are trademarks of Electronic Arts; the
game files are not included.
