# Paperdoll pictures

Our own pictures for Black Gate's paperdoll, for the items and people Serpent Isle's art does
not cover. They ship with the program; no game art goes here.

The `gwc-*.png` pictures are Grant W. Chapman's art
(github.com/GrantWChapman/Ultima-VII-Artwork), used with his permission. They are converted from
his .shp files, with their hotspots, and their `pictures.txt` lines sit between the marked lines;
change them in the converter, not by hand.

## Files

- One PNG per frame: 8-bit indexed, in the games' palette 0 (both games have the same one).
  Index 255 is clear. Only the indexes are read; keep all 256 palette entries in order (turn off
  any "optimise palette" option in the paint program).
- `pictures.txt`: one line per frame:

  ```text
  # name          frame  x   y   file
  CLOAK_GREY      0      21  4   cloak-grey-0.png
  ```

  `name` becomes `DOLLPIC_<NAME>` in `game/main/dollpics.h` (letters, digits, `_`). `frame`
  runs from 0 with no gaps. `x y` is the hotspot: the pixel that lands on the doll's point for
  that part. Each name gets one of the picture numbers Serpent Isle leaves empty, in the order
  names first appear; code uses the name, never the number.

- `derived.txt`: pictures the game makes at run time from one of Serpent Isle's, read from the
  player's own files (no SI pixels are stored here): one line each, the name, the SI picture,
  the frames to leave empty (`2-7`, `0,3` or `-`) and colour runs to move (`118-131:73` moves
  118..131 to 73..86):

  ```text
  # name           SI picture  empty frames  colour runs moved
  DEATH_SCYTHE     93          -             118-131:73 136-144:79
  ```

## Adding a picture

1. Draw the frames. For size, frames and hotspot, study the Serpent Isle picture used the same
   way (written as PNGs from your own game files, to a scratch folder outside the repo):

   ```sh
   python3 ultima7-modern/tools/dollart.py reference <Serpent Isle folder> 128 /tmp/head
   python3 ultima7-modern/tools/dollart.py palette <either game folder> /tmp/palette.png
   ```

2. Add the lines to `pictures.txt` and run `python3 ultima7-modern/tools/dollart.py`. It writes
   `game/main/dollpics.{h,cpp}`; commit those with the PNGs. `--check` fails if they are stale.
3. Name the picture in code: items in `BgOwnPictures` in `game/main/dollart.cpp` (with the
   SI item whose arm pose and slot rules it follows), Black Gate
   companions' heads by NPC number in `game/main/dollheads.cpp`.

## Frames by use (as Serpent Isle's)

| Use | Frames | Example |
| --- | --- | --- |
| Body armour | 0 female body, 1 male body; arms 2-7: pose 0, 1, 2, female then male | 57 leather armour |
| Head | 0 bare, 1 under a helmet; the Avatar has a pair per skin colour | 128 male Avatar |
| Helmet, legs, feet | 0 female, 1 male | 59 helm |
| Neck | one frame per item frame | 0 amulet |
| Weapon or held item | 0 in the hand, 1 on the back or belt | 107 sword |

All parts of the body (base body, arms, body armour, legs) share one point, so their hotspots
must agree; a head's hotspot is the head's point.
