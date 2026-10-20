/* Serpent Isle SI.EXE, resident segment 72 (file offsets 0x0301ad to 0x030268, 187 bytes).
 * Borland C++ 2.0 -mm -O -G rebuilds it byte for byte.
 * Folder chosen by subsystem.
 */

#include "u7port.h"
#include "dosio.h"
#include "memapi.h"
#include "oops.h"
#include "type.h"
#include "gametype.h"

char *TypeAnimations = 0;
struct TypeInfo gItemTypeInfo[SHAPE_NUMBERS] = { { 0 } };    /* TFA.DAT, each game's */
/* The animation table of each other game's TFA.DAT. */
static char OtherTypeAnimations[MAX_GAMES][512];

/* read 3072 bytes into buf from the head of the named file, and the 512-byte table after them */
void LoadTfa(char *buf, char *name)
{
	int16_t fd;

	if (TypeAnimations == 0) {
		TypeAnimations = AllocateFarHeap(INT32_C(512), 0);
		if (TypeAnimations == 0)
			ReportOutOfFarMemory();
	}
	fd = DosOpen(name);
	if (fd < 0)
		ReportFileNotFound(name);
	DosRead(fd, INT32_C(0), INT32_C(3072), buf);
	DosRead(fd, -INT32_C(1), INT32_C(512), TypeAnimations);
	DosClose(fd);
}

/* Reads another game's TFA.DAT into its shapes' entries. */
void LoadOtherTfa(enum GameType game)
{
	int16_t fd = plat_game_file_open(game, "STATIC\\TFA.DAT");

	if (fd < 0)
		ReportFileNotFound("TFA.DAT");
	DosRead(fd, INT32_C(0), INT32_C(3072), &gItemTypeInfo[GameShape(game, 0)]);
	DosRead(fd, -INT32_C(1), INT32_C(512), OtherTypeAnimations[game]);
	DosClose(fd);
}

/* two types a byte, odd types in the high nibble */
int16_t GetTypeAnimation(uint16_t type)
{
	char *table = IsForeignShape(type) ? OtherTypeAnimations[ShapeGame(type)] : TypeAnimations;

	type &= 0x3ff;
	if (type & 1)
		return (table[type >> 1] >> 4) & 0xf;
	else
		return table[type >> 1] & 0xf;
}

void ResetTypeGlobals(void)
{
	TypeAnimations = 0;
	memset(gItemTypeInfo, 0, sizeof gItemTypeInfo);
	memset(OtherTypeAnimations, 0, sizeof OtherTypeAnimations);
}
