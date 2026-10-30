/* Serpent Isle SI.EXE, overlay segment 251 (file offsets 0x06d6a0 to 0x06dd27, 1671 bytes).
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

/* path: voolook.c */
#include "u7port.h"
#include <stdio.h>
#include "lowlevel.h"
#include "dosio.h"
#include "vooalloc.h"
#include "init.h"
#include "makemojo.h"
#include "oops.h"
#include "easyfile.h"
#include "ammo.h"
#include "armor.h"
#include "equip.h"
#include "monsters.h"
#include "ready.h"
#include "weapons.h"
#include "debug.h"
#include "voolook.h"
#include "rules.h"
#include "ucset.h"

struct ArmorRecord { uint16_t type; char unusedFields[8]; };

int16_t LoadedWeaponCount;
int16_t LoadedAmmoCount;
int16_t LoadedArmorCount;
int16_t LoadedMonsterCount;
int16_t LoadedReadyCount;

void ShapeLookup::computeChecksum()
{
	int32_t end;
	int32_t address;

	sum = 0;
	end = addr + 2048;
	for (address = addr; address < end; address += 4)
		sum += PeekLong(address);
}

void ShapeLookup::verifyChecksum(int16_t, int16_t)
{
	int32_t old;

	old = sum;
	computeChecksum();
	if (sum != old)
		AssertFail(__FILE__, 88);
}

void ShapeLookup::create(int16_t fill, int8_t newTag)
{
	int16_t i;
	int32_t address;

	tag = newTag;
	addr = AllocateVoodooMemory(&VoodooXmsBlock, INT32_C(2048));
	if (addr == 0)
		ReportOutOfVoodooMemory();
	for (i = 0, address = addr; i < 1024; i++, address += 2)
		PokeWord(address, fill);
	computeChecksum();
}

/* Rewrites Black Gate's READY.DAT codes as Serpent Isle's, for the engine's slots. */
static void RemapReadyCodes(void)
{
	ReadyRecord record;

	for (int16_t i = 1; i <= ReadyCount; i++) {
		ReadyRecords.read(i, &record);
		record.slot = SiReadyCode(record.type, record.slot);
		CopyFarToLinear(ReadyRecords.base + i * INT32_C(9), &record, INT32_C(9));
	}
	ReadyRecords.cached = -1;
}

/* Opens the item data files and builds the per-type lookups into them. */
void LoadItemDataFiles(void)
{
	int16_t i;
	uint16_t type;

	WeaponRecords.load(BuildPath(StaticPath, WeaponsFileName, 0));
	ArmorRecords.load(BuildPath(StaticPath, ArmorFileName, 0));
	MonsterRecords.load(BuildPath(StaticPath, MonstersFileName, 0));
	AmmoRecords.load(BuildPath(StaticPath, AmmoFileName, 0));
	ReadyRecords.load(BuildPath(StaticPath, ReadyFileName, 0));
	EquipRecords.load(BuildPath(StaticPath, EquipFileName, 0));
	LoadedWeaponCount = WeaponCount;
	LoadedAmmoCount = AmmoCount;
	LoadedArmorCount = ArmorCount;
	LoadedMonsterCount = MonsterCount;
	LoadedReadyCount = ReadyCount;
	WeaponLookup.create(0, 'w');
	ArmorLookup.create(0, 'a');
	MonsterLookup.create(0, 'm');
	AmmoLookup.create(0, 'A');
	ReadyLookup.create(0, 'r');
	for (i = 1; i <= WeaponCount; i++)
		WeaponLookup.set(WeaponRecords.get(i)->type, i);
	for (i = 1; i <= ArmorCount; i++)
		ArmorLookup.set(ArmorRecords.get(i)->type, i);
	for (i = 1; i <= MonsterCount; i++) {
		type = MonsterRecords.get(i)->type;
		MonsterLookup.set(type, i);
	}
	for (i = 1; i <= AmmoCount; i++)
		AmmoLookup.set(AmmoRecords.get(i)->type, i);
	for (i = 1; i <= ReadyCount; i++) {
		type = ReadyRecords.get(i)->type;
		ReadyLookup.set(type, i);
	}
	if (PlayingBlackGate())
		RemapReadyCodes();
}

/* Shape fields of another game's records name its shapes; -1 and below are codes. */
static void WidenShape(int16_t *shape, GameType game)
{
	if (*shape >= 0)
		*shape = GameShape(game, *shape);
}

/* Adds another game's records after a table's, as numbers count and up, and points the lookup's
 * entries for that game's shapes at them. fix widens a record's shape fields. */
static void AppendRecords(GameType game, const char *name, int32_t size, int32_t *base, int32_t *count,
	ShapeLookup *lookup, void (*fix)(uint8_t *record, GameType game))
{
	char path[32];
	uint8_t n = 0;
	uint8_t record[32];
	int32_t newBase, newCount, i;
	int16_t fd;

	snprintf(path, sizeof path, "STATIC\\%s", name);
	fd = plat_game_file_open(game, path);
	if (fd < 0 || DosRead(fd, INT32_C(0), INT32_C(1), &n) != 1)
		ReportFileNotFound(path);
	if (!MakeMojo(*count + n, size, &newBase, &newCount))
		ReportOutOfVoodooMemory();
	for (i = 0; i < *count; i++) {
		CopyLinearToFar(record, *base + i * size, size);
		CopyFarToLinear(newBase + i * size, record, size);
	}
	for (i = 0; i < n; i++) {
		DosRead(fd, -INT32_C(1), size, record);
		fix(record, game);
		CopyFarToLinear(newBase + (*count + i) * size, record, size);
		lookup->set(*(uint16_t *)record, *count + i);
	}
	DosClose(fd);
	*base = newBase;
	*count = newCount;
}

static void FixWeapon(uint8_t *p, GameType game)
{
	WeaponRecord *record = (WeaponRecord *)p;

	WidenShape(&record->type, game);
	WidenShape(&record->ammo, game);
	WidenShape(&record->projectile, game);
	/* hit effects run in the other game's own usecode */
	if (record->usecode)
		record->usecode = SetFunction(game, record->usecode);
}

static void FixAmmo(uint8_t *p, GameType game)
{
	AmmoRecord *record = (AmmoRecord *)p;

	WidenShape(&record->type, game);
	WidenShape(&record->family, game);
	WidenShape(&record->projectile, game);
}

static void FixReady(uint8_t *p, GameType game)
{
	ReadyRecord *record = (ReadyRecord *)p;

	WidenShape(&record->type, game);
	if (game == GAME_BG)
		record->slot = SiReadyCode(record->type, record->slot);
}

/* Shape only: armour and monster records. */
static void FixType(uint8_t *p, GameType game)
{
	WidenShape((int16_t *)p, game);
}

/* Adds another game's weapons, armour, monsters, ammunition and ready records. */
void LoadOtherItemData(GameType game)
{
	AppendRecords(game, "WEAPONS.DAT", 21, &WeaponRecords.base, &WeaponRecords.count, &WeaponLookup, FixWeapon);
	AppendRecords(game, "ARMOR.DAT", 10, &ArmorRecords.base, &ArmorRecords.count, &ArmorLookup, FixType);
	AppendRecords(game, "MONSTERS.DAT", 25, &MonsterRecords.base, &MonsterRecords.count, &MonsterLookup, FixType);
	AppendRecords(game, "AMMO.DAT", 13, &AmmoRecords.base, &AmmoRecords.count, &AmmoLookup, FixAmmo);
	AppendRecords(game, "READY.DAT", 9, &ReadyRecords.base, &ReadyRecords.count, &ReadyLookup, FixReady);
	WeaponRecords.cached = ArmorRecords.cached = MonsterRecords.cached = -1;
	AmmoRecords.cached = ReadyRecords.cached = -1;
}

/* Asserts that no lookup changed since it was built; both arguments go unused. */
void VerifyShapeLookups(int16_t unused1, int16_t unused2)
{
	WeaponLookup.verifyChecksum(unused1, unused2);
	ArmorLookup.verifyChecksum(unused1, unused2);
	MonsterLookup.verifyChecksum(unused1, unused2);
	AmmoLookup.verifyChecksum(unused1, unused2);
	ReadyLookup.verifyChecksum(unused1, unused2);
}

extern "C" void ResetVoolookGlobals(void)
{
	LoadedWeaponCount = 0;
	LoadedAmmoCount = 0;
	LoadedArmorCount = 0;
	LoadedMonsterCount = 0;
	LoadedReadyCount = 0;
}
