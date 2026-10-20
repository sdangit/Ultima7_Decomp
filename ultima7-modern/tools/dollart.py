#!/usr/bin/env python3
"""Our own paperdoll pictures: PNGs in assets/paperdoll/ to the game's shape format.

    python3 ultima7-modern/tools/dollart.py             rebuild game/main/dollpics.{h,cpp}
    python3 ultima7-modern/tools/dollart.py --check     fail if they are out of date
    python3 ultima7-modern/tools/dollart.py palette <game dir> out.png
    python3 ultima7-modern/tools/dollart.py reference <serpent dir> <picture> <out dir>

pictures.txt lists one frame per line: name, frame, hotspot x and y, PNG file. derived.txt
lists pictures the game makes at run time from one of Serpent Isle's, in the player's own
files: some frames left empty, some colour runs moved. Each name gets a picture number of its
own from the numbers Serpent Isle's PAPERDOL.VGA leaves empty, pictures.txt's first, in the
order names first appear. See assets/paperdoll/README.md.

The PNGs are 8-bit indexed in the game's palette 0; index 255 is clear. Only the indexes are
read, so the PNG's own colours do not matter. Standard library only.
"""
import argparse
import os
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
ASSETS = HERE / 'assets' / 'paperdoll'
MANIFEST = ASSETS / 'pictures.txt'
DERIVED = ASSETS / 'derived.txt'
OUT_H = HERE / 'game' / 'main' / 'dollpics.h'
OUT_CPP = HERE / 'game' / 'main' / 'dollpics.cpp'
CLEAR = 255
# Pictures Serpent Isle's PAPERDOL.VGA leaves empty, in the order they are handed out.
FREE = list(range(170, 200)) + [11, 27, 40, 55, 56, 79, 80, 82, 90, 119]


def read_png(path):
    """Width, height and rows of indexes of an 8-bit indexed PNG."""
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        sys.exit('%s: not a PNG' % path)
    pos, idat, head = 8, b'', None
    while pos < len(data):
        size, kind = struct.unpack_from('>I4s', data, pos)
        body = data[pos + 8:pos + 8 + size]
        pos += size + 12
        if kind == b'IHDR':
            head = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
    w, h, depth, colour, _, _, interlace = head
    if depth != 8 or colour != 3 or interlace:
        sys.exit('%s: must be 8-bit indexed, not interlaced' % path)
    raw = zlib.decompress(idat)
    rows, prev = [], bytearray(w)
    for y in range(h):
        kind, line = raw[y * (w + 1)], bytearray(raw[y * (w + 1) + 1:(y + 1) * (w + 1)])
        for x in range(w):
            a = line[x - 1] if x else 0
            b, c = prev[x], prev[x - 1] if x else 0
            if kind == 1:
                line[x] = (line[x] + a) & 255
            elif kind == 2:
                line[x] = (line[x] + b) & 255
            elif kind == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return w, h, rows


def write_png(path, w, h, rows, palette):
    """An 8-bit indexed PNG; palette is 768 bytes, index 255 clear."""
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    raw = b''.join(b'\0' + bytes(r) for r in rows)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
    png += chunk(b'PLTE', bytes(palette)) + chunk(b'tRNS', b'\xff' * CLEAR + b'\0')
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    Path(path).write_bytes(png)


def encode_frame(w, h, rows, hx, hy):
    """One frame: its extents from the hotspot, then each row's runs of colour, then 0. The
    extents always take in the hotspot, which may lie outside the picture."""
    out = struct.pack('<hhhh', max(w - 1, hx) - hx, hx - min(0, hx), hy - min(0, hy), max(h - 1, hy) - hy)
    for y in range(h):
        x = 0
        while x < w:
            if rows[y][x] == CLEAR:
                x += 1
                continue
            end = x
            while end < w and rows[y][end] != CLEAR:
                end += 1
            out += struct.pack('<Hhh', (end - x) << 1, x - hx, y - hy) + bytes(rows[y][x:end])
            x = end
    return out + b'\0\0'


def encode_shape(frames):
    """Total size, each frame's offset, then the frames."""
    offset = 4 + 4 * len(frames)
    table, body = b'', b''
    for f in frames:
        table += struct.pack('<I', offset + len(body))
        body += f
    return struct.pack('<I', offset + len(body)) + table + body


def read_manifest():
    pictures = {}   # name -> {frame: (hx, hy, file)}
    for n, line in enumerate(MANIFEST.read_text().splitlines(), 1):
        words = line.split('#')[0].split()
        if not words:
            continue
        if len(words) != 5 or not words[0].replace('_', '').isalnum():
            sys.exit('pictures.txt:%d: want: name frame x y file' % n)
        name, frame, hx, hy, file = words[0].upper(), int(words[1]), int(words[2]), int(words[3]), words[4]
        pictures.setdefault(name, {})[frame] = (hx, hy, file)
    return pictures


def span(text):
    """'a-b' or 'a' as (a, b)."""
    a, _, b = text.partition('-')
    return int(a), int(b or a)


def read_derived():
    """name -> (Serpent Isle picture, empty frame bits, [(first, last, new first)])."""
    derived = {}
    for n, line in enumerate(DERIVED.read_text().splitlines(), 1):
        words = line.split('#')[0].split()
        if not words:
            continue
        if len(words) < 3:
            sys.exit('derived.txt:%d: want: name picture empty-frames [first-last:new]...' % n)
        bits = 0
        if words[2] != '-':
            for part in words[2].split(','):
                a, b = span(part)
                bits |= ((1 << (b + 1)) - 1) & ~((1 << a) - 1)
        moves = []
        for word in words[3:]:
            if word == '-':
                continue
            run, _, new = word.partition(':')
            moves.append(span(run) + (int(new),))
        if len(moves) > 4:
            sys.exit('derived.txt:%d: four colour runs at most' % n)
        derived[words[0].upper()] = (int(words[1]), bits, moves)
    return derived


def build():
    pictures = read_manifest()
    derived = read_derived()
    if len(pictures) + len(derived) > len(FREE):
        sys.exit('%d pictures, room for %d' % (len(pictures) + len(derived), len(FREE)))
    h = ['/* Made by tools/dollart.py from assets/paperdoll; do not edit. */',
         '#ifndef DOLLPICS_H', '#define DOLLPICS_H', '', '#include <stdint.h>', '']
    cpp = ['/* Made by tools/dollart.py from assets/paperdoll; do not edit. */',
           '#include "dollpics.h"', '']
    table = []
    for (name, frames), number in zip(pictures.items(), FREE):
        if sorted(frames) != list(range(len(frames))):
            sys.exit('%s: frames must run 0 to %d' % (name, len(frames) - 1))
        encoded = []
        for f in range(len(frames)):
            hx, hy, file = frames[f]
            w, ht, rows = read_png(ASSETS / file)
            encoded.append(encode_frame(w, ht, rows, hx, hy))
        data = encode_shape(encoded)
        h.append('#define DOLLPIC_%s %d' % (name, number))
        cpp.append('static const uint8_t %s[%d] = {' % (name.lower(), len(data)))
        for i in range(0, len(data), 24):
            cpp.append('\t' + ', '.join('%d' % b for b in data[i:i + 24]) + ',')
        cpp += ['};', '']
        table.append('\t{ %d, %s, sizeof %s },' % (number, name.lower(), name.lower()))
    made = []
    for (name, (source, bits, moves)), number in zip(derived.items(), FREE[len(pictures):]):
        h.append('#define DOLLPIC_%s %d' % (name, number))
        runs = ', '.join('{ %d, %d, %d }' % m for m in moves) or '{ 0, 0, 0 }'
        made.append('\t{ %d, %d, 0x%x, %d, { %s } },' % (number, source, bits, len(moves), runs))
    h += ['', '/* Our picture numbered so, its size in bytes, or 0. */',
          'const uint8_t *OwnDollPicture(int16_t picture, int32_t *size);', '',
          '/* A picture made at run time from a Serpent Isle one: the frames whose bits are set in',
          ' * emptyFrames left empty, and each colour run first..last moved to start at to. */',
          'struct DerivedDollPicture {', '\tint16_t picture, source;', '\tuint32_t emptyFrames;',
          '\tuint8_t moveCount;', '\tstruct { uint8_t first, last, to; } moves[4];', '};', '',
          '/* How the picture numbered so is made, or 0. */',
          'const struct DerivedDollPicture *FindDerivedDollPicture(int16_t picture);', '',
          '#endif', '']
    cpp += ['static const struct { int16_t picture; const uint8_t *data; int32_t size; } Pictures[] = {']
    cpp += table or ['\t{ -1, 0, 0 },']
    cpp += ['};', '',
            'const uint8_t *OwnDollPicture(int16_t picture, int32_t *size)', '{',
            '\tfor (uint16_t i = 0; i < sizeof Pictures / sizeof Pictures[0]; i++) {',
            '\t\tif (Pictures[i].picture == picture) {',
            '\t\t\t*size = Pictures[i].size;', '\t\t\treturn Pictures[i].data;', '\t\t}', '\t}',
            '\treturn 0;', '}', '']
    cpp += ['static const struct DerivedDollPicture Derived[] = {']
    cpp += made or ['\t{ -1, 0, 0, 0, { { 0, 0, 0 } } },']
    cpp += ['};', '',
            'const struct DerivedDollPicture *FindDerivedDollPicture(int16_t picture)', '{',
            '\tfor (uint16_t i = 0; i < sizeof Derived / sizeof Derived[0]; i++)',
            '\t\tif (Derived[i].picture == picture)', '\t\t\treturn &Derived[i];',
            '\treturn 0;', '}', '']
    return '\n'.join(h), '\n'.join(cpp)


def find(folder, *names):
    """A file under folder, matched without regard to case, as DOS names are."""
    for name in names:
        hits = [c for c in Path(folder).iterdir() if c.name.lower() == name.lower()]
        if not hits:
            sys.exit('%s not found in %s' % (name, folder))
        folder = hits[0]
    return folder


def flex_entry(path, n):
    data = path.read_bytes()
    count = struct.unpack_from('<I', data, 0x54)[0]
    if n >= count:
        return b''
    offset, size = struct.unpack_from('<II', data, 0x80 + 8 * n)
    return data[offset:offset + size]


def game_palette(game_dir):
    raw = flex_entry(find(game_dir, 'STATIC', 'PALETTES.FLX'), 0)
    if len(raw) == 1536:
        raw = bytes(raw[2 * i] for i in range(768))
    return [min(255, b * 4) for b in raw[:768]]


def decode_frame(shape, off):
    """Rows of indexes (255 clear) and the hotspot of one frame."""
    xr, xl, ya, yb = struct.unpack_from('<hhhh', shape, off)
    w, h = xl + xr + 1, ya + yb + 1
    rows = [bytearray([CLEAR]) * w for _ in range(h)]
    p = off + 8
    while True:
        scan = struct.unpack_from('<H', shape, p)[0]
        if scan == 0:
            break
        length, runs = scan >> 1, scan & 1
        dx, dy = struct.unpack_from('<hh', shape, p + 2)
        p += 6
        x, y = xl + dx, ya + dy
        if not runs:
            rows[y][x:x + length] = shape[p:p + length]
            p += length
            continue
        while length > 0:
            b = shape[p]
            run = b >> 1
            if b & 1:
                rows[y][x:x + run] = bytes([shape[p + 1]]) * run
                p += 2
            else:
                rows[y][x:x + run] = shape[p + 1:p + 1 + run]
                p += 1 + run
            x += run
            length -= run
    return w, h, rows, xl, ya


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--check', action='store_true', help='fail if the generated files are out of date')
    sub = ap.add_subparsers(dest='command')
    p = sub.add_parser('palette', help="a 16x16 swatch PNG of a game's palette 0, for a paint program")
    p.add_argument('game_dir')
    p.add_argument('out')
    r = sub.add_parser('reference', help="write a Serpent Isle picture's frames as PNGs, to study")
    r.add_argument('serpent_dir')
    r.add_argument('picture', type=int)
    r.add_argument('out_dir')
    args = ap.parse_args()

    if args.command == 'palette':
        write_png(args.out, 16, 16, [bytearray(range(16 * y, 16 * y + 16)) for y in range(16)],
                  game_palette(args.game_dir))
        return
    if args.command == 'reference':
        shape = flex_entry(find(args.serpent_dir, 'STATIC', 'PAPERDOL.VGA'), args.picture)
        if not shape:
            sys.exit('picture %d is empty' % args.picture)
        pal = game_palette(args.serpent_dir)
        os.makedirs(args.out_dir, exist_ok=True)
        count = (struct.unpack_from('<I', shape, 4)[0] - 4) // 4
        for f in range(count):
            w, h, rows, hx, hy = decode_frame(shape, struct.unpack_from('<I', shape, 4 + 4 * f)[0])
            name = os.path.join(args.out_dir, 'si-%d-%d.png' % (args.picture, f))
            write_png(name, w, h, rows, pal)
            print('%s  hotspot %d %d' % (name, hx, hy))
        return

    header, source = build()
    if args.check:
        if OUT_H.read_text() != header or OUT_CPP.read_text() != source:
            sys.exit('game/main/dollpics.{h,cpp} are out of date: run tools/dollart.py')
        return
    OUT_H.write_text(header)
    OUT_CPP.write_text(source)


if __name__ == '__main__':
    main()
