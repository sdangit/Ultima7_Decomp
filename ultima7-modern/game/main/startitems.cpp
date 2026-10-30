/* Items from another game that a game gets once (rules' StartItems), and which have been given. */

#include "u7port.h"
#include <string.h>
#include "plat.h"
#include "dosio.h"
#include "easyfile.h"
#include "item.h"
#include "npcref.h"
#include "type.h"
#include "wihh.h"
#include "search.h"
#include "rules.h"
#include "startitems.h"

char *const StartItemsFileName = "STARTITM.DAT";

/* one bit per entry of the played game's list */
static uint32_t StartItemsGiven;

/* Written only once something has been given, so saves without these items keep the same files. */
void SaveStartItems(char *dir)
{
	int16_t fd;

	if (!StartItemsGiven) {
		plat_file_remove(BuildPath(dir, StartItemsFileName, 0));
		return;
	}
	fd = CreateFileOrFail(BuildPath(dir, StartItemsFileName, 0));
	DosWrite(fd, -INT32_C(1), INT32_C(4), &StartItemsGiven);
	DosClose(fd);
}

void LoadStartItems(char *dir)
{
	int16_t fd;

	StartItemsGiven = 0;
	if (!plat_file_exists(BuildPath(dir, StartItemsFileName, 0)))
		return;
	fd = OpenFileOrFail(BuildPath(dir, StartItemsFileName, 0));
	DosRead(fd, -INT32_C(1), INT32_C(4), &StartItemsGiven);
	DosClose(fd);
}

/* A locked chest is a chest once opened. */
static uint8_t SameContainer(uint16_t type, uint16_t shape)
{
	return type == shape || ((type == 522 || type == 800) && (shape == 522 || shape == 800));
}

/* The container lying at the entry's cell, if its region is loaded. */
static objref FindContainer(const StartItem &entry)
{
	int16_t slot = FindRegionSlot(entry.x, entry.y);
	uint8_t x = entry.x & 0xff, y = entry.y & 0xff;

	if (slot == 255)
		return 0;
	for (objref item = ChunkItemLists[slot][y >> 4][x >> 4]; item.valid(); item = item.next()) {
		ItemRecord *rec = ITEM(item.off);

		if (rec->position.world.x == x && rec->position.world.y == y
			&& SameContainer(item.type(), entry.container))
			return item;
	}
	return 0;
}

void PlaceStartItems(void)
{
	uint16_t count;
	const StartItem *list = StartItems(&count);

	for (uint16_t i = 0; i < count; i++) {
		const StartItem &entry = list[i];
		uint16_t shape = GameShape(entry.game, entry.shape);
		objref container, item;
		AreaSearch search;

		if (StartItemsGiven & (UINT32_C(1) << i))
			continue;
		if (entry.place == START_IN_PACK) {
			container = objref(GetItemInSlot(AvatarRef, 15));
			if (!container.valid())
				container = AvatarRef;
		} else
			container = FindContainer(entry);
		if (!container.valid())
			continue;
		/* a save that already has one there keeps just that */
		if (!FindItemInContainer(&search, entry.place == START_IN_PACK ? AvatarRef : container, 0, shape, -1,
			entry.frame)) {
			if (!CreateItemInContainer(&item, TypeFrame(shape, entry.frame), container))
				continue;
			Item_setOkayToTake(&item);
			if (entry.quality)
				Item_setQuality(&item, (int8_t)entry.quality);
		}
		StartItemsGiven |= UINT32_C(1) << i;
	}
}

extern "C" void ResetStartitemsGlobals(void)
{
	StartItemsGiven = 0;
}
