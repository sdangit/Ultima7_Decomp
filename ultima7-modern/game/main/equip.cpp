/* Serpent Isle SI.EXE, overlay segment 219 (file offsets 0x05c860 to 0x05d709, 3753 bytes).
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

/* path: equip.c */
#include "u7port.h"
#include "objref.h"
#include "lowlevel.h"
#include "dosio.h"
#include "itemrec.h"
#include "iteminfo.h"
#include "npcref.h"
#include "vooalloc.h"
#include "cast.h"
#include "itembuf.h"
#include "itemovr1.h"
#include "makemojo.h"
#include "usehook.h"
#include "wihh.h"
#include "u7npc.h"
#include "type.h"
#include "coord.h"
#include "voolook.h"
#include "equip.h"
#include "ready.h"
#include "script.h"
#include "actqueue.h"
#include "init.h"
#include "rules.h"

#define NUM_EQUIP_SLOTS 18

/* the address of an NPC's equipment slot in the equipment list */
#define EQUIP_SLOT(npc, slot) (EquipList + (npc) * (NUM_EQUIP_SLOTS * 2) + (slot) * 2)

#define ITEM(off) ((struct ItemRecord *)ItemAt((off)))

MemoryAddress EquipList;

extern uint8_t Item_getQuantity(objref *ref);
extern int8_t Item_delete(objref *ref);
extern objref Item_getContainer(objref *);
extern uint8_t Item_moveIntoContainer(objref *ref, objref container);
extern void Item_markEquipped(objref *);
extern void Item_setWeaponReady(objref *);

#define CLASS_FLAGS(p) (ItemTypeClassFlags[ShapeInfo(Item_shape(RecordItem(p)))->typeClass])
#define IS_NPC(p) ((uint8_t)((CLASS_FLAGS(p) & CLASS_NPC) != 0))
#define HAS_CONTENTS(p) ((uint8_t)(CLASS_FLAGS(p) & CLASS_CONTENTS))

extern Coord Item_getX(objref &);
extern Coord Item_getY(objref &);
extern uint8_t Item_move(objref *, Loc, Loc, int16_t);
extern void Item_move(objref *, uint8_t, int16_t);

void QueueUnequipScript(ItemId item)
{
	uint8_t script[128];
	script[0] = 1;
	AppendScriptByte(script, 35);
	AppendScriptByte(script, 85);
	AppendScriptWord(script, 31003);
	ActionQueue.add(item.off, (char *)script);
}

uint8_t HasEquipUsecode(objref item)
{
	uint16_t type;
	if (!item.valid())
		return 0;
	type = Item_shape(item);
	return ShapeHasEquipScript(type);
}

void InitEquipment(void)
{
	int16_t slots[NUM_EQUIP_SLOTS];
	int16_t i;
	EquipList.address = AllocateVoodooMemory(&VoodooXmsBlock,
		(uint16_t)((NpcRecordCount + ExtraNpcRecordCount) * sizeof slots));
	if ((uint8_t)(EquipList.address == 0)) ReportError(0xe901);
	for (i = 0; i < NUM_EQUIP_SLOTS; i++) slots[i] = 0;
	for (i = 0; i < NpcRecordCount + ExtraNpcRecordCount; i++)
		CopyFarToLinear(EquipList + (int32_t)(i * sizeof slots), slots, (int32_t)sizeof slots);
}

/* Saves made before Black Gate's cloak went to the cape slot have it on the body. */
void MoveBgCloaksToCape(void)
{
	if (!PlayingBlackGate())
		return;
	for (int16_t i = 0; i < NpcRecordCount + ExtraNpcRecordCount; i++) {
		uint16_t off = PeekWord(EQUIP_SLOT(i, 12));

		if (off != 0 && IsBgCloak(Item_shape(off)) && PeekWord(EQUIP_SLOT(i, 2)) == 0) {
			PokeWord(EQUIP_SLOT(i, 2), off);
			PokeWord(EQUIP_SLOT(i, 12), 0);
		}
	}
}

uint8_t EquipItem(ItemId item, ItemId wearer, uint8_t slot, uint8_t combine)
{
	objref ref, unusedOwner;
	uint8_t occupied = 1;
	int16_t npcNum;
	ref.off = item.off;
	unusedOwner.off = wearer.off;
	npcNum = Item_getNpcNumber(&NPCRef(wearer.off));
	if (npcNum != -1) {
		/* Slot 20 takes both hands. */
		if (slot == 20)
			occupied = PeekWord(EQUIP_SLOT(npcNum, 1)) != 0 || PeekWord(EQUIP_SLOT(npcNum, 0)) != 0;
		else if (slot < NUM_EQUIP_SLOTS)
			occupied = PeekWord(EQUIP_SLOT(npcNum, slot)) != 0;
		else if (slot == NUM_EQUIP_SLOTS)
			HaltWithMessage(__FILE__, 214, "tried to clear out the EquipList[NUM_EQUIP_SLOTS]!");
		if (occupied) {
			if (combine) {
				objref existing = GetItemInSlot(wearer, slot);
				if (CanStackWith(&existing, item.off)) {
					Item_setQuantity(existing, Item_getQuantity(&ref) + Item_getQuantity(&existing), 0);
					Item_delete(&ref);
					return 1;
				}
			}
			return 0;
		} else {
			if (Item_getContainer(&ref) != wearer.off || (int8_t)(GetItemKind(&ref) == 7)) {
				Item_moveIntoContainer(&ref, wearer.off);
			}
			if (slot == 20) {
				PokeWord(EQUIP_SLOT(npcNum, 1), item.off);
				PokeWord(EQUIP_SLOT(npcNum, 0), item.off);
			} else
				PokeWord(EQUIP_SLOT(npcNum, slot), item.off);
			Item_markEquipped(&ref);
			if (slot == 1 || slot == 20) {
				Item_setWeaponReady(&NPCRef(wearer.off));
			}
			RunEquipUsecode(ref);
			return 1;
		}
	}
	return 0;
}

void RunEquipUsecode(objref ref)
{
	if (HasEquipUsecode(ref)) {
		if (!QueuesEquipScripts(Item_shape(ref)))
			RunUsable(5, ref.off, -1);
		else if (((IsGameShape(Item_shape(ref), GAME_SI, 296) &&
			ref.frame() == 0) ||
			(IsGameShape(Item_shape(ref), GAME_SI, 1013) &&
			ref.frame() == 0)) && !(uint8_t)UsableRunning)
			RunUsable(5, ref.off, -1);
		else if (!IsGameShape(Item_shape(ref), GAME_SI, 806)) {
			uint8_t script[128];
			script[0] = 1;
			AppendScriptByte(script, 35);
			AppendScriptByte(script, 85);
			AppendScriptWord(script, 31002);
			ActionQueue.add(ref.off, (char *)script);
		}
	}
}

uint8_t CanEquipInSlot(objref item, ItemId wearer, uint8_t slot, uint8_t combine)
{
	uint8_t needed;
	uint16_t type;
	type = Item_shape(item);
	needed = ReadyRecords.get(ReadyLookup.get(type))->slot;
	if (combine) {
		objref existing;
		existing = objref(GetItemInSlot(wearer, slot));
		if (existing.valid()) return CanStackWith(&existing, item);
	}
	if ((int8_t)ShapeInfo(type)->light && slot != 1 && slot != 0 && !LitShapeFitsAnySlot(type)) return 0;
	if (Item_shape(item) == 704 && (slot == 1 || slot == 0)) return 0;
	if ((needed == 8 || needed == 5 || needed == 6) && (slot == 1 || slot == 0)) return 0;
	if ((slot == 1 || slot == 0) && needed != 20) needed = slot;
	if (slot == 11 || slot == 15 || slot == 16 || slot == 17) {
		/* the belt and back slots take set lists */
		if (SlotTakesShape(slot, type))
			return objref(GetItemInSlot(wearer, slot)).valid() ? 0 : 1;
		return 0;
	} else {
		switch (needed) {
		case 0: case 2: case 3: case 4: case 5: case 6: case 7:
		case 9: case 10: case 12: case 13: case 14: case 15:
		{
			if (needed == slot) return objref(GetItemInSlot(wearer, slot)).valid() ? 0 : 1;
			return 0;
		}
		case 1:
			if (slot == 1 || slot == 0) return objref(GetItemInSlot(wearer, slot)).valid() ? 0 : 1;
			return 0;
		case 20:
			if (objref(GetItemInSlot(wearer, 1)).valid() != 0 ||
				objref(GetItemInSlot(wearer, 0)).valid() != 0) return 0;
			if (slot == 1 || slot == 0 || slot == 20) return 1;
			return 0;
		case 8:
			if (slot == 8 || slot == 7) return objref(GetItemInSlot(wearer, slot)).valid() ? 0 : 1;
			return 0;
		}
	}
	return 0;
}

void UnequipItem(ItemId id)
{
	objref item = id.off;
	objref owner, container;
	if (!item.valid()) return;
	if (!(uint8_t)(GetItemKind(&item) >= 6)) return;
	owner = Item_getContainer(&item);
	Item_moveIntoContainer(&item, owner);
	if (!IS_NPC(ITEM(owner.off))) return;
	if (!(uint8_t)((GetNpcBufferForIbo(&NPCRef(owner))->status & NPC_IN_PARTY) != 0)) return;
	uint8_t i = 0;
	uint8_t preferredSlots[3] = {15, 11, 17};
	for (; i < 3; i++) {
		switch (i) {
		case 0:
			container = objref(GetItemInSlot(owner, preferredSlots[i]));
			if (container.valid() && HAS_CONTENTS(ITEM(container.off))) {
				Item_moveIntoContainer(&item, container);
				return;
			}
			break;
		case 1: case 2:
			container = objref(GetItemInSlot(owner, preferredSlots[i]));
			if (container.valid() && HAS_CONTENTS(ITEM(container.off))) {
				Item_moveIntoContainer(&item, container);
				return;
			}
			if (CanEquipInSlot(item, NPCRef(owner), preferredSlots[i], 0)) {
				EquipItem(item, NPCRef(owner), preferredSlots[i], 0);
				return;
			}
			break;
		}
	}
	Item_move(&item, Item_getX(owner), Item_getY(owner), Item_getZ(&owner));
}

extern "C" void ResetEquipGlobals(void)
{
	memset(&EquipList, 0, sizeof(EquipList));
}
