/* Serpent Isle SI.EXE, resident segment 64 (file offsets 0x02ab9b to 0x02c44e, 6323 bytes).
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include "lowlevel.h"
#include "typefram.h"
#include "item.h"
#include "coord.h"
#include "u7npc.h"
#include "debug.h"
#include "itembuf.h"
#include "makemojo.h"
#include "combat.h"
#include "monsters.h"
#include "type.h"
#include "voolook.h"
#include "equip.h"
#include "voonpc.h"
#include "npcref.h"
#include "rules.h"
#include <new>

#define ITEM_TYPE(i) Item_shape(RecordItem(i))
#define TYPE_CLASS(i) (ShapeInfo(ITEM_TYPE(i))->typeClass)
#define IS_NPC(i) ((uint8_t)((ItemTypeClassFlags[TYPE_CLASS(i)] & CLASS_NPC) != 0))
#define IS_CLASS(i, n) ((uint8_t)(TYPE_CLASS(i) == (n)))
#define NPC_FLAG(n, mask) ((uint8_t)(((n)->status & (mask)) != 0))
#define NPC_EXTRA(off) ((NpcExtra *)ItemAt(off))

/* the class flags of TYPE_CLASS_HUMAN */
#define HUMAN_CLASS_FLAGS 0x3ee

/* set in scheduleFlags when the NPC's target is a place, not an item */
#define TARGET_IS_PLACE 0x200

/* in typeFlagsHigh: might, which doubles a statistic and lifts a weak one to 10 */
#define NPC_MIGHT       0x10

/* an NPC's extra record: its number sits where other items keep flags */
struct NpcExtra {
	int16_t contents;
	uint8_t region, quality;
	uint8_t npcNumber;
};

/* What SI packs into NPC stat bytes besides the stats, by NPC number. SI keeps NPC 0's mana and
 * magic in the low bits of those bytes, and every other NPC's flags and id there instead. */
struct NpcStatBits {
	uint8_t strength;     /* bits 5-7: skin colour, freeze */
	uint8_t intelligence; /* bits 5-7: read, tournament, polymorph */
	uint8_t combat;       /* bits 5-7: combat low, Petra */
	uint8_t temperature;  /* 0-63: the top bits of mana, then of magic */
	uint8_t flags;        /* in mana: met, no cast, zombie */
	uint8_t id;           /* in magic */
};

extern int16_t NpcItemRefs[];

const int16_t FacingFrameOffsets[] = { 0, 48, 16, 32 };
objref AvatarRef;
int16_t FreeNpcNumbers = -1;
int16_t FreeMonsterNumbers = -1;
NpcBufferPool NpcPool;
NpcBuffer NullNpcBuffer = { 0 };
static NpcStatBits NpcStats[NPC_COUNT];
static NpcStatBits NullNpcStats;

/* one 7-byte record of the NPC pose table */
void StoreNpcViewRecord(int32_t *address, int16_t index, NpcView *record)
{
	if (index >= 0 && index < NpcRecordCount + ExtraNpcRecordCount)
		CopyFarToLinear(*address + (uint16_t)(index * 7), record, INT32_C(7));
}

NpcBuffer * GetNpcBufferForIbo(objref *ref)
{
	if (ref->valid() && IS_NPC(ITEM(ref->off)))
		return GetCachedNpcBuffer(&NpcPool, Item_getNpcNumber(ref));
	return &NullNpcBuffer;
}

/* Pages the NPC in, as any stat read does, so the cache's random evictions stay in step. */
static NpcStatBits *GetNpcStats(objref *ref)
{
	if (GetNpcBufferForIbo(ref) == &NullNpcBuffer)
		return &NullNpcStats;
	return &NpcStats[Item_getNpcNumber(ref)];
}

/* The low five bits of SI's mana byte: NPC 0's mana, any other NPC's flags. Black Gate NPCs
 * all have mana. */
static uint8_t &ManaBits(objref *ref)
{
	NpcStatBits *bits = GetNpcStats(ref);

	return bits == NpcStats || !PacksNpcStats() ? GetNpcBufferForIbo(ref)->mana : bits->flags;
}

/* The low five bits of SI's magic byte: NPC 0's magic, any other NPC's id. */
static uint8_t &MagicBits(objref *ref)
{
	NpcStatBits *bits = GetNpcStats(ref);

	return bits == NpcStats || !PacksNpcStats() ? GetNpcBufferForIbo(ref)->magic : bits->id;
}

/* The met, no cast and zombie flags; SI shares them with mana. */
static uint8_t &FlagBits(objref *ref)
{
	return PacksNpcStats() ? ManaBits(ref) : GetNpcStats(ref)->flags;
}

/* SI shares the id with magic. */
static uint8_t &IdBits(objref *ref)
{
	return PacksNpcStats() ? MagicBits(ref) : GetNpcStats(ref)->id;
}

/* Joins a record's stats and flags into SI's packed bytes. */
void PackNpcStats(NpcBuffer *npc, int16_t number)
{
	NpcStatBits *bits = &NpcStats[number];

	if (!PacksNpcStats())
		return;
	npc->strength |= bits->strength;
	npc->intelligence |= bits->intelligence;
	npc->combat |= bits->combat;
	if (number != 0) {
		npc->mana = bits->flags;
		npc->magic = bits->id;
	}
	npc->mana |= bits->temperature << 5;
	npc->magic |= (bits->temperature & 0x38) << 2;
}

/* Splits SI's packed bytes into plain stats and the NPC's flags. */
void UnpackNpcStats(NpcBuffer *npc, int16_t number)
{
	NpcStatBits *bits = &NpcStats[number];

	if (!PacksNpcStats()) {
		memset(bits, 0, sizeof *bits);
		return;
	}
	bits->strength = npc->strength & 0xe0;
	bits->intelligence = npc->intelligence & 0xe0;
	bits->combat = npc->combat & 0xe0;
	bits->temperature = npc->mana >> 5 | (npc->magic & 0xe0) >> 2;
	npc->strength &= 31;
	npc->intelligence &= 31;
	npc->combat &= 31;
	npc->mana &= 31;
	npc->magic &= 31;
	bits->flags = 0;
	bits->id = 0;
	if (number != 0) {
		bits->flags = npc->mana;
		bits->id = npc->magic;
		npc->mana = 0;
		npc->magic = 0;
	}
}

void Npc_allocateNumber(objref *ref)
{
	int16_t number = FreeNpcNumbers;

	if (number != -1) {
		FreeNpcNumbers = GetCachedNpcBuffer(&NpcPool, number)->nextFree;
		Npc_setNumber(ref, number);
	}
}

void Npc_freeNumber(objref *ref)
{
	int16_t number = Item_getNpcNumber(ref);

	NpcItemRefs[number] = 0;
	GetCachedNpcBuffer(&NpcPool, number)->nextFree = FreeNpcNumbers;
	FreeNpcNumbers = number;
}

void Npc_allocateMonsterNumber(objref *ref)
{
	int16_t number = FreeMonsterNumbers;

	if (number != -1) {
		FreeMonsterNumbers = GetCachedNpcBuffer(&NpcPool, number)->nextFree;
		Npc_setNumber(ref, number);
	}
}

void Npc_freeMonsterNumber(objref *ref)
{
	int16_t number = Item_getNpcNumber(ref);

	NpcItemRefs[number] = 0;
	GetCachedNpcBuffer(&NpcPool, number)->nextFree = FreeMonsterNumbers;
	FreeMonsterNumbers = number;
}

uint8_t Npc_getStrength(objref *ref)
{
	int16_t value = GetNpcBufferForIbo(ref)->strength;

	if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_CURSED))
		value -= 3;
	if (GetNpcBufferForIbo(ref)->marked(NPC_MIGHT)) {
		if (value < 5)
			value = 10;
		else
			value *= 2;
	}
	if (value < 1)
		value = 1;
	if (value > 30 && GetNpcBufferForIbo(ref)->strength <= 30)
		value = 30;
	return value;
}

uint8_t Npc_getDexterity(objref *ref)
{
	int16_t value;

	value = GetNpcBufferForIbo(ref)->dexterity;
	if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_PARALYZED) || NPC_FLAG(GetNpcBufferForIbo(ref), NPC_DEAD)
		|| NPC_FLAG(GetNpcBufferForIbo(ref), NPC_ASLEEP) || (int8_t)Item_getHitPoints(ref) <= 0) {
		value = 0;
	} else {
		if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_CURSED))
			value -= 3;
		if (GetNpcBufferForIbo(ref)->marked(NPC_MIGHT)) {
			if (value < 5)
				value = 10;
			else
				value *= 2;
		}
	}
	if (value < 0)
		value = 0;
	if (value > 30)
		value = 30;
	return value;
}

uint8_t Npc_getIntelligence(objref *ref)
{
	int16_t value;

	value = GetNpcBufferForIbo(ref)->intelligence;
	if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_DEAD)) {
		value = 1;
	} else {
		if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_CURSED))
			value -= 3;
		if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_CHARMED))
			value--;
		if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_ASLEEP))
			value--;
		if (GetNpcBufferForIbo(ref)->marked(NPC_MIGHT)) {
			if (value < 5)
				value = 10;
			else
				value *= 2;
		}
	}
	if (value < 1)
		value = 1;
	if (value > 30)
		value = 30;
	return value;
}

uint8_t Npc_getCombat(objref *ref)
{
	int16_t value;

	value = GetNpcBufferForIbo(ref)->combat;
	if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_PARALYZED) || NPC_FLAG(GetNpcBufferForIbo(ref), NPC_DEAD)
		|| NPC_FLAG(GetNpcBufferForIbo(ref), NPC_ASLEEP) || (int8_t)Item_getHitPoints(ref) <= 0) {
		value = 1;
	} else {
		if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_CURSED))
			value -= 3;
		if (GetNpcBufferForIbo(ref)->marked(NPC_MIGHT)) {
			if (value < 5)
				value = 10;
			else
				value *= 2;
		}
	}
	if (value < 1)
		value = 1;
	if (value > 30 && GetNpcBufferForIbo(ref)->combat <= 30)
		value = 30;
	return value;
}

int16_t Npc_getLevel(objref *ref)
{
	int16_t experience;
	int16_t level = 1;

	for (experience = GetNpcBufferForIbo(ref)->experience / INT32_C(100);
		experience != 0; experience = experience >> 1)
		level++;
	return level;
}

int16_t Item_isAvatar(objref *ref)
{
	if (ref->off == AvatarRef.off)
		return 1;
	return 0;
}

/* Creates an NPC at x, y, z: a human takes a numbered NPC slot, anything else a monster slot. Its
 * equipment slots start empty and it moves as its MONSTERS.DAT record says. */
uint8_t CreateNpc(objref *ref, TypeFrame &shape, CellCoord x, CellCoord y, int16_t z)
{
	int16_t classFlags;
	int16_t monsterIndex = MonsterLookup.get(shape.type());
	int16_t slots[18];

	for (int16_t i = 0; i < 18; i++)
		slots[i] = 0;
	classFlags = ItemTypeClassFlags[ShapeInfo(shape.type())->typeClass];
	if (classFlags == HUMAN_CLASS_FLAGS) {
		if (FreeNpcNumbers != -1) {
			if (CreateItem(ref, TypeFrame(shape.bits), x, y, z)) {
				Npc_allocateNumber(ref);
				GetNpcBufferForIbo(ref)->status = 0;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = 0;
				GetNpcBufferForIbo(ref)->setType(shape);
				CopyFarToLinear(EquipList + Item_getNpcNumber(ref) * (int32_t)sizeof slots, slots, (int32_t)sizeof slots);
				if ((uint8_t)MonsterRecords.get(monsterIndex)->walk) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_WALK;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->swim) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_SWIM;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->fly) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_FLY;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->ethereal) {
					if ((uint8_t)ShapeInfo(ITEM_TYPE(ITEM(ref->off)))->solid) {
						GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_FLY;
					} else {
						GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_ETHEREAL;
					}
				}
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~0x40;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~0x80;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~4;
				Npc_clearMetFlag(ref);
				Npc_clearNoCastFlag(ref);
				Npc_clearPolymorphFlag(ref);
				Npc_clearTournamentFlag(ref);
				Npc_clearZombieFlag(ref);
				Npc_setTemperature(ref, 0);
				Npc_setId(ref, 0);
				Item_setQualityFlags(ref, 0);
				CombatGroups.movePoints[Item_getNpcNumber(ref)] = 0;
				return 1;
			}
		}
	} else {
		if (FreeMonsterNumbers != -1) {
			if (CreateItem(ref, TypeFrame(shape.bits), x, y, z)) {
				Npc_allocateMonsterNumber(ref);
				GetNpcBufferForIbo(ref)->status = 0;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = 0;
				GetNpcBufferForIbo(ref)->setType(shape);
				CopyFarToLinear(EquipList + Item_getNpcNumber(ref) * (int32_t)sizeof slots, slots, (int32_t)sizeof slots);
				if ((uint8_t)MonsterRecords.get(monsterIndex)->walk) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_WALK;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->swim) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_SWIM;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->fly) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_FLY;
				}
				if ((uint8_t)MonsterRecords.get(monsterIndex)->ethereal) {
					GetNpcBufferForIbo(ref)->typeFlags = GetNpcBufferForIbo(ref)->typeFlags | NPC_ETHEREAL;
				}
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~0x40;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~0x80;
				GetNpcBufferForIbo(ref)->typeFlagsHigh = GetNpcBufferForIbo(ref)->typeFlagsHigh & ~4;
				Npc_clearMetFlag(ref);
				Npc_clearNoCastFlag(ref);
				Npc_clearPolymorphFlag(ref);
				Npc_clearTournamentFlag(ref);
				Npc_clearZombieFlag(ref);
				Npc_setTemperature(ref, 0);
				Npc_setId(ref, 0);
				Item_setQualityFlags(ref, 0);
				CombatGroups.movePoints[Item_getNpcNumber(ref)] = 0;
				return 1;
			}
		}
	}
	return 0;
}

void Item_moveOffMap(objref *ref)
{
	if (Item_detach(ref) != 0)
		PlaceItemOffMap(ref);
}

void GetNpcIbo(objref *ref, int16_t number)
{
	if ((uint16_t)(NpcRecordCount + ExtraNpcRecordCount) > (uint16_t)number)
		ref->off = NpcItemRefs[number];
	else
		ref->off = 0;
}

int16_t Item_getNpcNumber(objref *ref)
{
	int16_t extra;

	if (IS_NPC(ITEM(ref->off))) {
		extra = ITEM(ref->off)->data.extra;
		if (IS_CLASS(ITEM(ref->off), TYPE_CLASS_MONSTER))
			return NPC_EXTRA(extra)->npcNumber + NpcRecordCount;
		else
			return NPC_EXTRA(extra)->npcNumber;
	}
	return -1;
}

void Npc_setNumber(objref *ref, uint16_t number)
{
	int16_t extra;

	if (IS_NPC(ITEM(ref->off)) && number < (uint16_t)(NpcRecordCount + ExtraNpcRecordCount)) {
		extra = ITEM(ref->off)->data.extra;
		if (IS_CLASS(ITEM(ref->off), TYPE_CLASS_MONSTER))
			NPC_EXTRA(extra)->npcNumber = number - NpcRecordCount;
		else
			NPC_EXTRA(extra)->npcNumber = number;
		NpcItemRefs[number] = ref->off;
	}
}

/* An NPC's target is an item, or with TARGET_IS_PLACE a place: x and y offsets from the
 * avatar in schedulePosition's two bytes and z in scheduleFlags' top nibble. */
uint8_t Npc_hasItemTarget(objref *ref)
{
	return !((GetNpcBufferForIbo(ref)->scheduleFlags & TARGET_IS_PLACE) >> 9);
}

int16_t Npc_getItemTarget(objref *ref)
{
	if (GetNpcBufferForIbo(ref)->scheduleFlags & TARGET_IS_PLACE)
		return 0;
	return GetNpcBufferForIbo(ref)->schedulePosition;
}

void Npc_getTargetCoords(objref *ref, int16_t *x, int16_t *y, int16_t *z)
{
	int16_t d;

	if (GetNpcBufferForIbo(ref)->scheduleFlags & TARGET_IS_PLACE) {
		d = GetNpcBufferForIbo(ref)->schedulePosition & 0xff;
		if (d > 127)
			d |= 0xff00;
		*x = (int16_t)Item_getX(AvatarRef) + d;
		d = (GetNpcBufferForIbo(ref)->schedulePosition & 0xff00) >> 8;
		if (d > 127)
			d |= 0xff00;
		*y = (int16_t)Item_getY(AvatarRef) + d;
		d = GetNpcBufferForIbo(ref)->scheduleFlags >> 12 & 0xf;
		*z = d;
	} else {
		objref target = GetNpcBufferForIbo(ref)->schedulePosition;

		*x = Item_getX(target);
		*y = Item_getY(target);
		*z = GetItemZAndStuff(&target).z();
	}
}

int16_t Npc_getTargetWeapon(objref *ref)
{
	int16_t value = GetNpcBufferForIbo(ref)->scheduleFlags & 0x1ff;

	if (value & 0x100)
		value |= 0xfe00;
	return value;
}

void Npc_setItemTarget(objref *ref, int16_t target)
{
	GetNpcBufferForIbo(ref)->schedulePosition = target;
	GetNpcBufferForIbo(ref)->scheduleFlags &= ~TARGET_IS_PLACE;
}

void Npc_setCoordTarget(objref *ref, Coord x, Coord y, int16_t z)
{
	int16_t dx = x.value - Item_getX(AvatarRef);
	int16_t dy = y.value - Item_getY(AvatarRef);

	GetNpcBufferForIbo(ref)->schedulePosition = (dx & 0xff) | (dy << 8 & 0xff00);
	GetNpcBufferForIbo(ref)->scheduleFlags = (GetNpcBufferForIbo(ref)->scheduleFlags & 0x1ff) |
		(z << 12 & 0xf000) | TARGET_IS_PLACE;
}

void Npc_setTargetWeapon(objref *ref, uint16_t value)
{
	GetNpcBufferForIbo(ref)->scheduleFlags = (GetNpcBufferForIbo(ref)->scheduleFlags & 0xfe00) | (value & 0x1ff);
}

void Npc_setMale(objref *ref)
{
	GetNpcBufferForIbo(ref)->typeFlagsHigh |= 2;
}

void Npc_setFemale(objref *ref)
{
	uint8_t flags = GetNpcBufferForIbo(ref)->typeFlagsHigh;

	GetNpcBufferForIbo(ref)->typeFlagsHigh = flags & ~2;
}

uint8_t Npc_isMale(objref *ref)
{
	return GetNpcBufferForIbo(ref)->typeFlagsHigh & 2;
}

uint8_t IsNpcUnconscious(objref *ref)
{
	if (NPC_FLAG(GetNpcBufferForIbo(ref), NPC_PARALYZED)
		|| NPC_FLAG(GetNpcBufferForIbo(ref), NPC_ASLEEP)
		|| (uint8_t)((int8_t)Item_getHitPoints(ref) <= 0)
		|| NPC_FLAG(GetNpcBufferForIbo(ref), NPC_DEAD))
		return 1;
	return 0;
}

void Npc_changeFood(objref *ref, int8_t amount)
{
	int16_t food = GetNpcBufferForIbo(ref)->food + amount;

	if (food >= 31)
		GetNpcBufferForIbo(ref)->food = 31;
	else if (food < 0)
		GetNpcBufferForIbo(ref)->food = 0;
	else
		GetNpcBufferForIbo(ref)->food = food;
}

uint8_t Npc_hasMetFlag(objref *ref)
{
	return (FlagBits(ref) & 1) != 0;
}

void Npc_setMetFlag(objref *ref)
{
	FlagBits(ref) |= 1;
}

void Npc_clearMetFlag(objref *ref)
{
	FlagBits(ref) &= ~1;
}

uint8_t Npc_hasNoCastFlag(objref *ref)
{
	return (FlagBits(ref) & 2) != 0 && !(uint8_t)Item_isAvatar(ref);
}

void Npc_setNoCastFlag(objref *ref)
{
	FlagBits(ref) |= 2;
}

void Npc_clearNoCastFlag(objref *ref)
{
	FlagBits(ref) &= ~2;
}

uint8_t Npc_getSkinColor(objref *ref)
{
	return (GetNpcStats(ref)->strength & 0x60) >> 5;
}

void Npc_setSkinColor(objref *ref, uint8_t value)
{
	GetNpcStats(ref)->strength = value << 5;
}

uint8_t Npc_hasZombieFlag(objref *ref)
{
	return (FlagBits(ref) & 4) != 0 && !(uint8_t)Item_isAvatar(ref);
}

void Npc_setZombieFlag(objref *ref)
{
	FlagBits(ref) |= 4;
}

void Npc_clearZombieFlag(objref *ref)
{
	FlagBits(ref) &= ~4;
}

uint8_t Npc_hasFreezeFlag(objref *ref)
{
	return GetNpcStats(ref)->strength & 0x80;
}

void Npc_setFreezeFlag(objref *ref)
{
	GetNpcStats(ref)->strength |= 0x80;
}

void Npc_clearFreezeFlag(objref *ref)
{
	GetNpcStats(ref)->strength &= ~0x80;
}

uint8_t Npc_hasReadFlag(objref *ref)
{
	return GetNpcStats(ref)->intelligence & 0x20;
}

void Npc_setReadFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence |= 0x20;
}

void Npc_clearReadFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence &= ~0x20;
}

uint8_t Npc_hasPetraFlag(objref *ref)
{
	return GetNpcStats(ref)->combat & 0x80;
}

void Npc_setPetraFlag(objref *ref)
{
	GetNpcStats(ref)->combat |= 0x80;
}

void Npc_clearPetraFlag(objref *ref)
{
	GetNpcStats(ref)->combat &= ~0x80;
}

uint8_t Npc_hasCombatLowFlag(objref *ref)
{
	return GetNpcStats(ref)->combat & 0x40;
}

void Npc_setCombatLowFlag(objref *ref)
{
	GetNpcStats(ref)->combat |= 0x40;
}

void Npc_clearCombatLowFlag(objref *ref)
{
	GetNpcStats(ref)->combat &= ~0x40;
}

uint8_t Npc_hasPolymorphFlag(objref *ref)
{
	return GetNpcStats(ref)->intelligence & 0x80;
}

void Npc_setPolymorphFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence |= 0x80;
}

void Npc_clearPolymorphFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence &= ~0x80;
}

uint8_t Npc_hasTournamentFlag(objref *ref)
{
	return GetNpcStats(ref)->intelligence & 0x40;
}

void Npc_setTournamentFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence |= 0x40;
}

void Npc_clearTournamentFlag(objref *ref)
{
	GetNpcStats(ref)->intelligence &= ~0x40;
}

uint8_t Npc_getTemperature(objref *ref)
{
	return GetNpcStats(ref)->temperature;
}

void Npc_setTemperature(objref *ref, uint8_t value)
{
	GetNpcStats(ref)->temperature = value & 63;
}

void Npc_incrementTemperature(objref *ref)
{
	if (Npc_getTemperature(ref) < 63)
		Npc_setTemperature(ref, Npc_getTemperature(ref) + 1);
}

void Npc_decrementTemperature(objref *ref)
{
	if (Npc_getTemperature(ref) > 0)
		Npc_setTemperature(ref, Npc_getTemperature(ref) - 1);
}

uint8_t Npc_packedManaLowRange(objref *ref)
{
	return Npc_getTemperature(ref) > 14 && Npc_getTemperature(ref) < 25;
}

uint8_t Npc_packedManaMiddleRange(objref *ref)
{
	return Npc_getTemperature(ref) > 24 && Npc_getTemperature(ref) < 40;
}

uint8_t Npc_packedManaUpperRange(objref *ref)
{
	return Npc_getTemperature(ref) > 39 && Npc_getTemperature(ref) < 50;
}

uint8_t Npc_packedManaHighRange(objref *ref)
{
	return Npc_getTemperature(ref) > 50 && Npc_getTemperature(ref) < 64;
}

/* SI stats are five bits; any more spill into the flags above them, as in SI's packed byte. */
void Npc_setBaseStrength(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		GetNpcBufferForIbo(ref)->strength = value;
		return;
	}
	GetNpcBufferForIbo(ref)->strength = value & 31;
	GetNpcStats(ref)->strength |= value & 0xe0;
}

void Npc_setBaseIntelligence(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		GetNpcBufferForIbo(ref)->intelligence = value;
		return;
	}
	GetNpcBufferForIbo(ref)->intelligence = value & 31;
	GetNpcStats(ref)->intelligence |= value & 0xe0;
}

void Npc_setBaseCombat(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		GetNpcBufferForIbo(ref)->combat = value;
		return;
	}
	GetNpcBufferForIbo(ref)->combat = value & 31;
	GetNpcStats(ref)->combat |= value & 0xe0;
}

uint8_t Npc_getMana(objref *ref)
{
	return ManaBits(ref);
}

void Npc_setMana(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		ManaBits(ref) = value;
		return;
	}
	ManaBits(ref) = value & 31;
	GetNpcStats(ref)->temperature |= value >> 5;
}

uint8_t Npc_getMagic(objref *ref)
{
	return MagicBits(ref);
}

void Npc_setMagic(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		MagicBits(ref) = value;
		return;
	}
	MagicBits(ref) = value & 31;
	GetNpcStats(ref)->temperature |= (value & 0xe0) >> 2;
}

uint8_t Npc_getId(objref *ref)
{
	return IdBits(ref);
}

void Npc_setId(objref *ref, uint8_t value)
{
	if (!PacksNpcStats()) {
		IdBits(ref) = value;
		return;
	}
	Npc_setMagic(ref, value);
}

extern "C" void ResetNpcrefGlobals(void)
{
	AvatarRef = 0;
	FreeNpcNumbers = -1;
	FreeMonsterNumbers = -1;
	memset(&NullNpcBuffer, 0, sizeof NullNpcBuffer);
	memset(NpcStats, 0, sizeof NpcStats);
	memset(&NullNpcStats, 0, sizeof NullNpcStats);
}

extern "C" void ConstructNpcrefGlobals(void)
{
	new (&NpcPool) NpcBufferPool();
}
