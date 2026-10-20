# Ultima7 Decompile

This is an AI assisted decompilation of the Ultima 7 game code. Original names were used wherever they were available, but the majority of the function and variable names are inferred, however the logic itself was tested to produce byte exact results using Borland C++ 2.0, the compiler originally used to build Ultima 7.

The `decompiled` folder contains the source code for the various binaries in Ultima 7: The Black Gate and Ultima 7 Part Two: Serpent Isle. A lot of the original source was assembly code, which was faithfully preserved. It seems that they also used .c extensions for their C++ code, so I kept that convention.

The `ultima7-portable` folder is an attempt to clean up the decompiled code to make it more cross-platform friendly with minimal changes to the logic, so that the original games can be played on modern machines. Black Gate and Serpent Isle are both built as drop-in replacement binaries. Each merges the multiple programs for the intro, endgame, menu, and main game, clears the globals between each load, attempts to catch or fix null pointer exceptions that were valid in dos, 16 to 32 bit issues, and etc. For audio, you can use either the integrated [Munt](https://github.com/munt/munt) by providing MT32 roms, or using the integrated [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) for adlib support. The other cross platform functions are handled by the [Matoya](https://github.com/snowcone-ltd/libmatoya) library. If you want the original experience, warts and all, on a modern PC, this is what you are looking for.

The `ultima7-modern` port is an adaptation of the Serpent Isle engine modified to support the Black Gate as well. This applies all of Serpent Isle's bugfixes and quality of life changes to black gate as well. The parts of BG's engine that were removed were restored. For Serpent Isle's paperdoll system, [Grant Chapman's art](https://github.com/GrantWChapman/Ultima-VII-Artwork) was used with his permission.
Some features in this version:
- 16:9 and 16:10 aspect ratios for the viewport.
- OPL3 overlay inferred from the MT32 music, so you can hear the Adlib music in stereo.
- Items from both games can coexist in each other game if desired, so you can have a keyring in U7, for instance.
- In Black Gate, Ring of Reagents, Helm of Light, and Everlasting Goblet have been hidden in secret places around the world as QoL. Use them or not as you wish.
- In Black Gate, The avatar starts with a keyring in his or her pack.
- Paperdolls, hotkeys like Feed, Target, and etc in black gate.
- More gradual shifts for lighting effects.
- A start menu with settings.
- Relaxed NPC movement and animations closer to the 5fps the game seems to intend these elements to move at.

This is still from the original decompiled source and doesn't change the game mechanics significantly. Mostly this fork is just trying to improve quality of life for modern hardware. This is also not intended to be an engine for modding and replacing games. For that, use the amazing [Exult engine](https://github.com/exult). This is just a slightly more modern way to play the original two games. If you want one as close to the original as possible however, use the portable releases individually.

The `ultima7-usecode` folder is tools for decompiling and building and working with U7 usecode. They haven't really been tested on anything but a mac.
