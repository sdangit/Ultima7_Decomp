#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "art.h"

#define FLEX_COUNT 0x54
#define FLEX_TABLE 0x80

static uint32_t Read32(const uint8_t *p)
{
	return p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static int16_t Read16(const uint8_t *p)
{
	return (int16_t) (p[0] | p[1] << 8);
}

ArtEntry ArtLoad(int16_t game, const char *name, int16_t index)
{
	ArtEntry entry = {0, 0};
	uint8_t bytes[8];
	int16_t file = plat_game_file_open(game, name);

	if (file < 0)
		return entry;
	if (plat_file_seek(file, FLEX_COUNT, PLAT_SEEK_SET) == FLEX_COUNT
		&& plat_file_read(file, bytes, 4) == 4 && (uint32_t) index < Read32(bytes)
		&& plat_file_seek(file, FLEX_TABLE + 8 * index, PLAT_SEEK_SET) >= 0
		&& plat_file_read(file, bytes, 8) == 8) {
		int32_t offset = (int32_t) Read32(bytes);
		int32_t size = (int32_t) Read32(bytes + 4);

		entry.data = size > 0 ? (uint8_t *) malloc((size_t) size) : 0;
		if (entry.data && plat_file_seek(file, offset, PLAT_SEEK_SET) == offset
			&& plat_file_read(file, entry.data, size) == size)
			entry.size = size;
	}
	plat_file_close(file);
	if (entry.size == 0)
		ArtFree(&entry);
	return entry;
}

void ArtFree(ArtEntry *entry)
{
	free(entry->data);
	entry->data = 0;
	entry->size = 0;
}

int16_t ArtPaletteFrom(const ArtEntry *entry, ArtPalette out)
{
	int16_t i;

	/* some palettes keep each level in a 16-bit word */
	if (entry->size == 768 * 2) {
		for (i = 0; i < 768; ++i)
			out[i] = entry->data[2 * i] & 63;
		return 1;
	}
	if (entry->size < 768)
		return 0;
	for (i = 0; i < 768; ++i)
		out[i] = entry->data[i] & 63;
	return 1;
}

int16_t ArtFrameCount(const ArtEntry *shape)
{
	if (shape->size < 8)
		return 0;
	return (int16_t) ((Read32(shape->data + 4) - 4) / 4);
}

/* The frame's header, or null when the shape has no such frame. */
static const uint8_t *FrameAt(const ArtEntry *shape, int16_t frame)
{
	uint32_t offset;

	if (frame < 0 || frame >= ArtFrameCount(shape))
		return 0;
	offset = Read32(shape->data + 4 + 4 * frame);
	if (offset + 8 > (uint32_t) shape->size)
		return 0;
	return shape->data + offset;
}

int16_t ArtFrameBounds(const ArtEntry *shape, int16_t frame, int16_t *left, int16_t *top,
	int16_t *right, int16_t *bottom)
{
	const uint8_t *header = FrameAt(shape, frame);

	if (!header)
		return 0;
	*right = Read16(header);
	*left = (int16_t) -Read16(header + 2);
	*top = (int16_t) -Read16(header + 4);
	*bottom = Read16(header + 6);
	return 1;
}

/* Walks the frame's spans. Each span is a row piece: a word of length << 1 | packed, the
 * offset from the origin, then the pixels, either plain or as runs of a byte of
 * count << 1 | repeat followed by one colour (repeat) or count colours. A pixel callback
 * gets the screen position and colour. */
typedef void (*PixelFn)(void *context, int16_t x, int16_t y, uint8_t colour);

static void WalkFrame(const ArtEntry *shape, int16_t frame, int16_t x0, int16_t y0,
	PixelFn pixel, void *context)
{
	const uint8_t *p = FrameAt(shape, frame);
	const uint8_t *end = shape->data + shape->size;
	uint16_t span;

	if (!p)
		return;
	p += 8;
	while (p + 2 <= end && (span = (uint16_t) Read16(p)) != 0) {
		int16_t length = (int16_t) (span >> 1);
		int16_t x, y, i;

		if (p + 6 > end)
			return;
		x = (int16_t) (x0 + Read16(p + 2));
		y = (int16_t) (y0 + Read16(p + 4));
		p += 6;
		if (!(span & 1)) {
			if (p + length > end)
				return;
			for (i = 0; i < length; ++i)
				pixel(context, (int16_t) (x + i), y, p[i]);
			p += length;
			continue;
		}
		while (length > 0 && p < end) {
			int16_t run = (int16_t) (*p >> 1);
			int16_t repeat = *p & 1;

			++p;
			if (p + (repeat ? 1 : run) > end)
				return;
			for (i = 0; i < run; ++i)
				pixel(context, (int16_t) (x + i), y, repeat ? *p : p[i]);
			p += repeat ? 1 : run;
			x = (int16_t) (x + run);
			length = (int16_t) (length - run);
		}
	}
}

static void MarkColour(void *context, int16_t x, int16_t y, uint8_t colour)
{
	(void) x;
	(void) y;
	((uint8_t *) context)[colour] = 1;
}

int16_t ArtAddColours(ArtScreen *screen, const ArtEntry *shape, const ArtPalette palette,
	uint8_t map[256])
{
	uint8_t used[256];
	int16_t frame, i;

	memset(used, 0, sizeof used);
	memset(map, 0, 256);
	for (frame = 0; frame < ArtFrameCount(shape); ++frame)
		WalkFrame(shape, frame, 0, 0, MarkColour, used);
	for (i = 0; i < 256; ++i) {
		if (!used[i])
			continue;
		if (screen->used >= 256)
			return 0;
		memcpy(screen->rgb + 3 * screen->used, palette + 3 * i, 3);
		map[i] = (uint8_t) screen->used++;
	}
	return 1;
}

typedef struct {
	ArtScreen *screen;
	const uint8_t *map;
	int16_t x, y;
	int16_t first, last;    /* frame rows drawn */
} DrawContext;

static void Plot(ArtScreen *screen, int16_t x, int16_t y, uint8_t colour)
{
	if (x >= 0 && x < screen->width && y >= 0 && y < screen->height)
		screen->pixels[y * screen->width + x] = colour;
}

static void PutPixel(void *context, int16_t x, int16_t y, uint8_t colour)
{
	DrawContext *draw = (DrawContext *) context;

	if (y >= draw->first && y <= draw->last)
		Plot(draw->screen, (int16_t) (draw->x + x), (int16_t) (draw->y + y), draw->map[colour]);
}

void ArtDrawRows(ArtScreen *screen, const ArtEntry *shape, int16_t frame, int16_t x, int16_t y,
	const uint8_t map[256], int16_t first, int16_t last)
{
	DrawContext draw;

	draw.screen = screen;
	draw.map = map;
	draw.x = x;
	draw.y = y;
	draw.first = first;
	draw.last = last;
	WalkFrame(shape, frame, 0, 0, PutPixel, &draw);
}

void ArtDraw(ArtScreen *screen, const ArtEntry *shape, int16_t frame, int16_t x, int16_t y,
	const uint8_t map[256])
{
	ArtDrawRows(screen, shape, frame, x, y, map, INT16_MIN, INT16_MAX);
}

void ArtBlit(ArtScreen *screen, const uint8_t *pixels, int16_t width, int16_t height,
	int16_t x, int16_t y, const uint8_t *map, uint8_t empty)
{
	int16_t col, row;

	for (row = 0; row < height; ++row) {
		for (col = 0; col < width; ++col) {
			uint8_t value = pixels[row * width + col];

			if (value != empty)
				Plot(screen, (int16_t) (x + col), (int16_t) (y + row), map[value]);
		}
	}
}

/* The menus print each character then step its width plus twice the spacing of 1. */
static int16_t CharAdvance(const ArtEntry *font, uint8_t c)
{
	int16_t left, top, right, bottom;

	if (!ArtFrameBounds(font, c, &left, &top, &right, &bottom))
		return 0;
	return (int16_t) (right - left + 1 + 2);
}

int16_t ArtTextWidth(const ArtEntry *font, const char *text)
{
	int16_t width = 0;

	for (; *text; ++text)
		width = (int16_t) (width + CharAdvance(font, (uint8_t) *text));
	return width > 0 ? (int16_t) (width - 2) : 0;
}

void ArtText(ArtScreen *screen, const ArtEntry *font, const uint8_t map[256], int16_t x,
	int16_t y, const char *text)
{
	for (; *text; ++text) {
		ArtDraw(screen, font, (uint8_t) *text, x, y, map);
		x = (int16_t) (x + CharAdvance(font, (uint8_t) *text));
	}
}
