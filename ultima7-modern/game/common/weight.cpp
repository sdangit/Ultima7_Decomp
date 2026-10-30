/* Serpent Isle SI.EXE, overlay segment 255 (file offsets 0x06ed80 to 0x06f058, 728 bytes).
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include "dosio.h"
#include "item.h"
#include "memapi.h"
#include "npcref.h"
#include "type.h"
#include "rules.h"

#define ITEM_TYPE(rec) Item_shape(RecordItem(rec))
#define TYPE_CLASS(rec) (ShapeInfo(ITEM_TYPE(rec))->typeClass)
#define CLASS_FLAGS(rec) (ItemTypeClassFlags[TYPE_CLASS(rec)])
#define HAS_CONTENTS(rec) ((int8_t) (CLASS_FLAGS(rec) & CLASS_CONTENTS))

#define TYPE_COUNT 1024

#define GOLD_COIN 644
#define REAGENT 842

/* weight and bulk of a type */
struct WeightVolume {
	uint8_t weight;
	uint8_t bulk;
};

WeightVolume *TypeWeightVolumes;
/* Each other game's WGTVOL.DAT. */
static WeightVolume OtherWeightVolumes[MAX_GAMES][TYPE_COUNT];

static WeightVolume *TypeWeightVolume(int16_t type)
{
	if (IsForeignShape(type))
		return &OtherWeightVolumes[ShapeGame(type)][type & 0x3ff];
	return &TypeWeightVolumes[type];
}

inline int8_t IsTypeClass(NPCRef r, int16_t typeClass)
{
	return TYPE_CLASS(ITEM(r.off)) == (uint16_t)typeClass;
}

int16_t GetItemTypeWeight(int16_t type)
{
	return TypeWeightVolume(type)->weight;
}

/* a stack weighs its quantity times its type's weight, a tenth of that for coins and reagents */
int16_t Item_getWeight(objref item)
{
	uint16_t type;
	int8_t stackable;
	int16_t weight;

	type = ITEM_TYPE(ITEM(item.off));
	stackable = ShapeInfo(type)->typeClass == TYPE_CLASS_QUANTITY;
	weight = GetItemTypeWeight(type);
	if (stackable) {
		weight *= (uint8_t)Item_getQuantity(&item);
		uint16_t n = type & 0x3ff;

		/* coins and reagents of either game */
		if (n == GOLD_COIN || n == REAGENT || (ShapeGame(type) == GAME_SI && (n == 951 || n == 948 || n == 952)))
			weight /= 10;
		if (weight == 0)
			weight = 1;
	}
	return weight;
}

int16_t GetItemTypeBulk(int16_t type)
{
	return TypeWeightVolume(type)->bulk;
}

void LoadWgtVol(char *name)
{
	int16_t fd;

	TypeWeightVolumes = (WeightVolume *) AllocateFarHeap(TYPE_COUNT * sizeof(WeightVolume), 0);
	if (TypeWeightVolumes == 0)
		ReportError(0xeb01);
	fd = DosOpen(name);
	if (fd < 0)
		ReportError(0xeb02);
	DosRead(fd, INT32_C(0), TYPE_COUNT * sizeof(WeightVolume), TypeWeightVolumes);
	DosClose(fd);
	if (HasRoomierPacks()) {
		TypeWeightVolumes[BACKPACK_TYPE].bulk = 220;
		TypeWeightVolumes[BAG_TYPE].weight = 2;
	}
}

int16_t DetermineWeightOfContents(objref item)
{
	int16_t weight = 0;

	if (item.valid() && (IsTypeClass(item, TYPE_CLASS_HUMAN) || HAS_CONTENTS(ITEM(item.off)))) {
		objref child = GetContainedItem(&item);

		while (child.valid()) {
			weight += Item_getWeight(child);
			weight += DetermineWeightOfContents(child);
			child = child.next();
		}
	}
	return weight;
}

int16_t DetermineBulkOfContents(objref item)
{
	int16_t bulk = 0;

	if (item.valid() && (IsTypeClass(item, TYPE_CLASS_HUMAN) || HAS_CONTENTS(ITEM(item.off)))) {
		objref child = GetContainedItem(&item);

		while (child.valid()) {
			bulk += GetItemTypeBulk(ITEM_TYPE(ITEM(child.off)));
			child = child.next();
		}
	}
	return bulk;
}

/* Reads another game's WGTVOL.DAT. */
void LoadOtherWgtVol(GameType game)
{
	int16_t fd = plat_game_file_open(game, "STATIC\\WGTVOL.DAT");

	if (fd < 0)
		ReportError(0xeb02);
	DosRead(fd, INT32_C(0), TYPE_COUNT * sizeof(WeightVolume), OtherWeightVolumes[game]);
	DosClose(fd);
}

extern "C" void ResetWeightGlobals(void)
{
	TypeWeightVolumes = 0;
	memset(OtherWeightVolumes, 0, sizeof OtherWeightVolumes);
}
