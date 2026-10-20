/* Black Gate's paperdoll. Its attack mode and protect buttons work as on Black Gate's own
 * inventory window.
 */

#include "u7port.h"
#include <stdio.h>
#include <string.h>
#include "lowlevel.h"
#include "u7manage.h"
#include "item.h"
#include "u7npc.h"
#include "u7event.h"
#include "gumpmgr.h"
#include "bltshape.h"
#include "itable.h"
#include "bogus.h"
#include "combat.h"
#include "crime.h"
#include "death.h"
#include "equip.h"
#include "text.h"
#include "use.h"
#include "usehook.h"
#include "weight.h"
#include "wihh.h"
#include "type.h"
#include "npcref.h"
#include "partymov.h"
#include "voolook.h"
#include "ready.h"
#include "rules.h"
#include "bginv.h"

BgPaperdoll::BgPaperdoll(NPCRef item)
	: InventoryGump(item), attackModeButton(BgGump(12)), protectButton(BgGump(7))
{
	int16_t number = NPCRef(Gump::object()).number();

	add(&attackModeButton);
	attackModeButton.show();
	if (IsAvatarInCombat())
		attackModeButton.setFrame(Item_getAttackMode(NPCRef(Gump::object())));
	else
		attackModeButton.setFrame(Item_getDefaultAttackMode(NPCRef(Gump::object())));
	add(&protectButton);
	protectButton.show();
	if (GetGroupLeader(1) == number && number != -1)
		protectButton.setFrame(1);
	moveTo(bounds.x, bounds.y);
}

/* The protect button: the companion the party guards in combat. */
uint8_t BgPaperdoll::setLeader(uint8_t on)
{
	if (!on) {
		ClearGroupLeader(1);
		ReleaseProtectors(1, -1);
		return GUMP_HANDLED;
	}
	if (attackModeButton.getFrame() == 4) {
		protectButton.setFrame(0);
		return GUMP_HANDLED;
	}
	if (GetPartySize() > 1) {
		int16_t old = GetGroupLeader(1);

		if (old != -1) {
			objref npc;
			GumpNode *found;

			GetNpcIbo(&npc, old);
			found = GetOpenItemDialogListNode(npc, 0);
			if (found && found->dialog != this)
				((BgPaperdoll *)found->dialog)->protectButton.setFrame(0);
		}
		SetGroupLeader(Item_getNpcNumber(&objref(Gump::object())));
	}
	return GUMP_HANDLED;
}

uint8_t BgPaperdoll::handle(MouseState *event)
{
	uint8_t result;

	if (event->action != MOUSE_CLICK || PickingItem || !bounds.contains(MouseState_getX(event), event->y))
		return InventoryGump::handle(event);
	if ((result = protectButton.handle(event)) != 0) {
		if (result == BUTTON_ON || result == BUTTON_OFF)
			return setLeader(result == BUTTON_ON);
		return GUMP_HANDLED;
	}
	if ((result = attackModeButton.handle(event)) == 0)
		return InventoryGump::handle(event);
	if (result == GUMP_HANDLED)
		return result;
	int16_t frame = attackModeButton.getFrame();
	if (frame == 4 && protectButton.getFrame() != 0) {
		frame++;
		attackModeButton.setFrame(frame);
	}
	if (!NPCRef(Gump::object()).isAvatar()) {
		if (frame == 9)
			attackModeButton.setFrame(0);
	} else {
		switch (frame) {
		case 3:
		case 4:
			attackModeButton.setFrame(5);
			break;
		case 6:
		case 7:
		case 8:
			attackModeButton.setFrame(9);
			break;
		}
	}
	objref npc = Gump::object();
	if ((uint16_t)attackModeButton.getFrame() != Item_getDefaultAttackMode(npc)) {
		Item_setDefaultAttackMode(npc, attackModeButton.getFrame());
		if (IsAvatarInCombat())
			Item_setAttackMode(npc, attackModeButton.getFrame());
	}
	return GUMP_HANDLED;
}

void BgPaperdoll::moveTo(int16_t x, int16_t y)
{
	InventoryGump::moveTo(x, y);
	protectButton.moveTo(x + 49, y + 21);
	attackModeButton.moveTo(x + 50, y + 43);
}
