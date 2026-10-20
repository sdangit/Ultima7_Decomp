#ifndef VOOLOOK_H
#define VOOLOOK_H

#include "lowlevel.h"
#include "makemojo.h"
#include "itemrec.h"

/* A table of 1024 words in voodoo memory, one per type, with a checksum to catch stray writes;
 * other games' types have their words in other. */
struct ShapeLookup {
	int8_t tag;
	int32_t addr;
	int32_t sum;
	uint16_t other[MAX_GAMES][1024];
	void computeChecksum();
	void verifyChecksum(int16_t, int16_t);
	void create(int16_t fill, int8_t tag);
	uint16_t get(uint16_t type)
	{
		if (IsForeignShape(type))
			return other[ShapeGame(type)][type & 0x3ff];
		CheckMojoBounds(INT32_C(1024), (int32_t) type);
		return PeekWord(addr + type * 2);
	}
	void set(uint16_t type, int16_t n)
	{
		if (IsForeignShape(type)) {
			other[ShapeGame(type)][type & 0x3ff] = n;
			return;
		}
		CheckMojoBounds(INT32_C(1024), (uint32_t) type);
		PokeWord(addr + type * 2, n);
		computeChecksum();
	}
};

extern ShapeLookup WeaponLookup;
extern ShapeLookup ArmorLookup;
extern ShapeLookup MonsterLookup;
extern ShapeLookup AmmoLookup;
extern ShapeLookup ReadyLookup;

/* The record numbers of an item's type in the weapon, armour, ammunition and monster tables. */
inline int16_t GetWeaponNumber(objref item) { return WeaponLookup.get(item.type()); }
inline int16_t GetArmorNumber(objref item) { return ArmorLookup.get(item.type()); }
inline int16_t GetAmmoNumber(objref item) { return AmmoLookup.get(item.type()); }
inline int16_t GetMonsterNumber(objref item) { return MonsterLookup.get(item.type()); }

extern int16_t LoadedWeaponCount;
extern int16_t LoadedAmmoCount;
extern int16_t LoadedArmorCount;
extern int16_t LoadedMonsterCount;
extern int16_t LoadedReadyCount;

void LoadItemDataFiles(void);
void LoadOtherItemData(GameType game);
void VerifyShapeLookups(int16_t unused1, int16_t unused2);

#endif
