#ifndef BGINV_H
#define BGINV_H

#include "gumps.h"
#include "npcref.h"

/* Black Gate's paperdoll: Serpent Isle's, with Black Gate's attack mode and protect buttons,
 * which Serpent Isle keeps on its combat status window. */
struct BgPaperdoll : InventoryGump {
	SpecialSprite attackModeButton;
	CounterSprite protectButton;
	BgPaperdoll(NPCRef);
	uint8_t setLeader(uint8_t on);
	uint8_t handle(MouseState *);
	void moveTo(int16_t, int16_t);
};

#endif
