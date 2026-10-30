/* Game art for the start menu: Flex entries, shapes and palettes read from either game's
 * folder, drawn into one 8-bit frame whose palette holds the colours of every picture.
 *
 *   BG palette -+                        +- logo map  -> screen slots 1..
 *   SI palette -+-> AddColours(shape) -> +- gate map  -> screen slots ..
 *   ...         |                        +- font map  -> screen slots ..
 *
 * Each picture keeps its own colours: AddColours gives every colour it uses a slot of the
 * screen palette and returns the map from its indices to those slots.
 */
#ifndef STARTMENU_ART_H
#define STARTMENU_ART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_BG 0
#define GAME_SI 1

#define FRAME_MAX_WIDTH 432

/* A whole Flex entry: a shape, a palette or anything else. */
typedef struct {
	uint8_t *data;
	int32_t size;
} ArtEntry;

typedef struct {
	uint8_t pixels[FRAME_MAX_WIDTH * 200];
	int16_t width, height;
	uint8_t rgb[256 * 3];       /* 6-bit VGA levels */
	int16_t used;               /* slots taken; slot 0 stays black */
} ArtScreen;

/* A palette of 256 6-bit triples. */
typedef uint8_t ArtPalette[256 * 3];

/* Reads one entry of a Flex file in the given game's folder. Zero size when it is missing. */
ArtEntry ArtLoad(int16_t game, const char *name, int16_t index);
void ArtFree(ArtEntry *entry);
/* Nonzero when the entry is a palette, which it copies. */
int16_t ArtPaletteFrom(const ArtEntry *entry, ArtPalette out);

int16_t ArtFrameCount(const ArtEntry *shape);
/* The frame's extent around its origin: left and top are zero or less. */
int16_t ArtFrameBounds(const ArtEntry *shape, int16_t frame, int16_t *left, int16_t *top,
	int16_t *right, int16_t *bottom);
/* Gives every colour the shape's frames use a screen slot; fills map. Zero when the
 * palette is full. */
int16_t ArtAddColours(ArtScreen *screen, const ArtEntry *shape, const ArtPalette palette,
	uint8_t map[256]);
/* Draws a frame with its origin at x, y, through map. */
void ArtDraw(ArtScreen *screen, const ArtEntry *shape, int16_t frame, int16_t x, int16_t y,
	const uint8_t map[256]);
/* Only the frame's rows first to last, counted from its origin. */
void ArtDrawRows(ArtScreen *screen, const ArtEntry *shape, int16_t frame, int16_t x, int16_t y,
	const uint8_t map[256], int16_t first, int16_t last);
/* Draws width by height pixels through map, skipping those equal to empty. */
void ArtBlit(ArtScreen *screen, const uint8_t *pixels, int16_t width, int16_t height,
	int16_t x, int16_t y, const uint8_t *map, uint8_t empty);

/* Text in a menu font: each character is that frame of the shape; y is the baseline. */
int16_t ArtTextWidth(const ArtEntry *font, const char *text);
void ArtText(ArtScreen *screen, const ArtEntry *font, const uint8_t map[256], int16_t x,
	int16_t y, const char *text);

#ifdef __cplusplus
}
#endif

#endif
