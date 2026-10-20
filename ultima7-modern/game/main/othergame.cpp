/* Items from the game that is not being played: its tables, and a test hook that makes some. */

#include "u7port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "gametype.h"
#include "item.h"
#include "npcref.h"
#include "type.h"
#include "weight.h"
#include "cullmask.h"
#include "wihh.h"
#include "voolook.h"
#include "weapons.h"
#include "armor.h"
#include "ready.h"
#include "cast.h"
#include "text.h"
#include "look.h"
#include "spell.h"
#include "othergame.h"
#include "ucset.h"

/* Reads what the other game's shapes need to be items here. */
void LoadOtherGames(void)
{
	for (int16_t i = GAME_BG; i <= GAME_SI; i++) {
		GameType game = (GameType)i;

		if (game == PlayedGame())
			continue;
		LoadOtherTfa(game);
		LoadOtherWgtVol(game);
		LoadOtherShapeExtents(game);
		LoadOtherWihh(game);
		LoadOtherItemData(game);
		LoadOtherUsecode(game);
	}
}

static void LogItem(objref item)
{
	struct ArmorStats { int16_t type; int8_t protection; };
	char line[200];
	uint16_t type = item.type();
	int16_t weapon = GetWeaponNumber(item);
	int16_t armor = GetArmorNumber(item);
	int16_t ready = ReadyLookup.get(type);

	char shown[80];

	ProduceItemDisplayName(shown, &item, Item_hasQuantity(&item) ? Item_getQuantity(&item) : 0);
	snprintf(line, sizeof line, "test item %u (game %d, shape %u) \"%s\" class %d weight %d bulk %d slot %d"
		" damage %d range %d armour %d\n", type, ShapeGame(type), type & 0x3ff, shown,
		ShapeInfo(type)->typeClass, Item_getWeight(item), GetItemTypeBulk(type),
		ready ? ReadyRecords.get(ready)->slot : -1, weapon ? WeaponRecords.get(weapon)->damage : -1,
		weapon ? WeaponRecords.get(weapon)->range : -1,
		armor ? ((ArmorStats *)ArmorRecords.get(armor))->protection : -1);
	plat_log(line);
}

/* Test hook: U7_TEST_CREATE lists items to make at the start, each
 * game:shape[/frame][xcount][qquality], as in "bg:635 si:644x20 +si:485 bg:641/3q77". They go on the ground south-west of the Avatar; a leading +
 * puts one in his pack. */
void CreateTestItems(void)
{
	const char *list = getenv("U7_TEST_CREATE");
	char name[3], line[40];
	int consumed;
	int16_t placed = 0;

	if (list == 0)
		return;
	snprintf(line, sizeof line, "test avatar at %d %d\n", Item_getX(AvatarRef).value, Item_getY(AvatarRef).value);
	plat_log(line);
	while (*list) {
		unsigned shape = 0, frame = 0, count = 0, quality = 0;
		uint8_t inPack = *list == '+';
		objref item;

		if (inPack)
			list++;
		consumed = 0;
		if (sscanf(list, "%2[a-z]:%u%n", name, &shape, &consumed) != 2)
			break;
		list += consumed;
		if (sscanf(list, "/%u%n", &frame, &consumed) == 1)
			list += consumed;
		if (sscanf(list, "x%u%n", &count, &consumed) == 1)
			list += consumed;
		if (sscanf(list, "q%u%n", &quality, &consumed) == 1)
			list += consumed;
		while (*list == ' ' || *list == ',')
			list++;
		TypeFrame typeFrame(GameShape(strcmp(name, "bg") == 0 ? GAME_BG : GAME_SI, (uint16_t)shape), frame);
		if (inPack) {
			objref pack = objref(GetItemInSlot(AvatarRef, 15));

			if (!CreateItemInContainer(&item, typeFrame, pack.valid() ? pack : AvatarRef))
				continue;
		} else {
			int16_t dx = -2 - 2 * (placed % 5), dy = 2 + 2 * (placed / 5);

			if (!CreateItem(&item, typeFrame, Coord(Item_getX(AvatarRef).value + dx),
				Coord(Item_getY(AvatarRef).value + dy), Item_getZ(&AvatarRef)))
				continue;
			placed++;
		}
		Item_setOkayToTake(&item);
		if (count)
			Item_setQuantity(item, count, 0);
		if (ShapeInfo(item.type())->typeClass == TYPE_CLASS_SPELLBOOK)
			for (uint8_t spell = 0; spell < 72; spell++)
				AddSpellToSpellbook(&item, spell);
		if (quality)
			Item_setQuality(&item, (int8_t) quality);
		LogItem(item);
	}
}
