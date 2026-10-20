/* Serpent Isle SI.EXE, resident segment 68 (file offsets 0x02d72e to 0x02d7d0, 162 bytes).
 * Borland C++ 2.0 -mm -O -G rebuilds it byte for byte.
 * Folder chosen by subsystem.
 */

#include "u7port.h"
#include "type.h"

/* a flipped item lies with its x and y swapped */
#define IS_FLIPPED(w) ((int8_t) (((w) & 0x8000) == 0x8000))
/* a TypeFrame: the word, then the type's bits above ten */
#define TYPE_OF(p) (((p)[0] & 0x3ff) | ((p)[1] << 10))

uint16_t GetFootprintX(uint16_t *typeFrame)
{
	if (IS_FLIPPED(*typeFrame))
		return ShapeInfo(TYPE_OF(typeFrame))->footprintY;
	else
		return ShapeInfo(TYPE_OF(typeFrame))->footprintX;
}

uint16_t GetFootprintY(uint16_t *typeFrame)
{
	if (IS_FLIPPED(*typeFrame))
		return ShapeInfo(TYPE_OF(typeFrame))->footprintX;
	else
		return ShapeInfo(TYPE_OF(typeFrame))->footprintY;
}
