/* Serpent Isle SI.EXE, overlay segment 321 (file offsets 0x08e7e0 to 0x08f868, 4232 bytes).
 * Borland C++ 2.0 -mm -O -P -Y rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include "arena.h"
#include <new>
#include <stdio.h>
#include "objref.h"
#include "itemrec.h"
#include "colbuf.h"
#include "gumps.h"
#include "u7npc.h"
#include "npcref.h"
#include "u7manage.h"
#include "bltshape.h"
#include "u7event.h"
#include "itable.h"
#include "text.h"
#include "init.h"
#include "partymov.h"
#include "party.h"
#include "combat.h"
#include "crime.h"
#include "use.h"
#include "item.h"
#include "rules.h"
#include "dollart.h"

extern objref AvatarRef;
extern View Viewport;

#define PARTY_SIZE ((int16_t)PartySize)

ProportionalTextPrinter CombatTextPrinter;

void CombatStatsGump::initialize()
{
	int16_t height, width;

	stats = 0;
	switch (PartySize) {
	case 1:
		shape = 1481;
		break;
	case 2:
		shape = 1482;
		break;
	case 3:
		shape = 1483;
		break;
	case 4:
		shape = 1484;
		break;
	case 5:
		shape = 1485;
		break;
	case 6:
	case 7:
	case 8:
		shape = 1486;
		break;
	default:
		FatalError("Hey, we got more than 8 people in the party!");
	}
	/* Black Gate's party can reach eight; the art has six columns */
	extraColumns = PartySize > 6 ? PartySize - 6 : 0;
	gShapeManager.getShapeSize(&width, &height, shape);
	width += extraColumns * 29;
	bounds.set(0, 0, width, height);
	add(&closeButton);
	closeButton.show();
	displayed = AvatarRef;
	displayed.off = 0;
	Control::show();
	CombatTextPrinter.setFont(2);
	int8_t i;
	for (i = 0; i < PARTY_SIZE; i++) {
		add(&faces[i]);
		faces[i].setFrame(0);
		add(&attackModeButtons[i]);
		add(&protectButtons[i]);
	}
	update();
	moveTo(ScreenShift() + width > ScreenWidth ? ScreenWidth - width : ScreenShift(), 0);
}

/* Each member's face, attack mode and protect button; the slots past the party are hidden. */
void CombatStatsGump::update()
{
	uint8_t i;
	int16_t leader;
	objref npc;

	for (i = 0; i < PARTY_SIZE; i++) {
		if (PlayingBlackGate()) {
			int16_t head = DollFace(NPCRef(PartyMembers[i]));

			faces[i].setShape(head >= 0 ? FIRST_PAPERDOLL_SHAPE + head : 1651);
			faces[i].setFrame(head >= 0 ? 0 : 4);
		} else if (i == 0) {
			if (Npc_hasPetraFlag(&AvatarRef))
				faces[0].setShape(1660);
			else {
				faces[0].setShape(Npc_isMale(&AvatarRef) ? 1651 : 1527);
				faces[0].setFrame(Npc_getSkinColor(&AvatarRef) * 2);
			}
		} else {
			switch (PartyMembers[i].type()) {
			case 488:
				faces[i].setShape(1647);
				break;
			case 487:
				faces[i].setShape(1649);
				break;
			case 465:
				faces[i].setShape(1648);
				break;
			case 658:
				if (Npc_hasPetraFlag(&AvatarRef)) {
					faces[i].setShape(Npc_isMale(&PartyMembers[i]) ? 1651 : 1527);
					faces[i].setFrame(Npc_getSkinColor(&AvatarRef) * 2);
				} else
					faces[i].setShape(1660);
				break;
			case 747:
				faces[i].setShape(1668);
				break;
			default:
				switch (Item_getNpcNumber(&PartyMembers[i])) {
				case 45:
					faces[i].setShape(1677);
					break;
				case 44:
					faces[i].setShape(1678);
					break;
				case 149:
					faces[i].setShape(1650);
					break;
				case 26:
					faces[i].setShape(1679);
					break;
				case 168:
					faces[i].setShape(1680);
					break;
				case 34:
					faces[i].setShape(1681);
					break;
				case 152:
					faces[i].setShape(1682);
					break;
				default:
					faces[i].setShape(1651);
					faces[i].setFrame(4);
					break;
				}
			}
		}
		faces[i].unlock();
		faces[i].show();
		faces[i].lock();
		attackModeButtons[i].setShape(1435);
		if (IsAvatarInCombat())
			attackModeButtons[i].setFrame(Item_getAttackMode(PartyMembers[i]));
		else
			attackModeButtons[i].setFrame(Item_getDefaultAttackMode(PartyMembers[i]));
		attackModeButtons[i].unlock();
		attackModeButtons[i].show();
		attackModeButtons[i].lock();
		protectButtons[i].setShape(1430);
		protectButtons[i].setFrame(0);
		protectButtons[i].unlock();
		protectButtons[i].show();
		protectButtons[i].lock();
	}
	leader = GetGroupLeader(1);
	if (leader != -1) {
		GetNpcIbo(&npc, leader);
		int16_t index = GetPartyIndex(npc);
		if (index < PARTY_SIZE)
			protectButtons[index].setFrame(1);
	}
	for (i = PARTY_SIZE; i < 8; i++) {
		faces[i].unlock();
		faces[i].hide();
		faces[i].lock();
		attackModeButtons[i].unlock();
		attackModeButtons[i].hide();
		attackModeButtons[i].lock();
		protectButtons[i].unlock();
		protectButtons[i].hide();
		protectButtons[i].lock();
	}
}

uint8_t CombatStatsGump::handle(MouseState *event)
{
	uint8_t result = 0;

	if (!isVisible())
		return 0;
	int16_t x = MouseState_getX(event);
	int16_t y = event->y;
	if (!bounds.contains(x, y))
		return 0;
	int16_t hit = gShapeManager.isCursorInBounds(shape, 0, bounds, Point(x, y))
		|| extraColumns && gShapeManager.isCursorInBounds(shape, 0, Point(bounds.x + extraColumns * 29, bounds.y), Point(x, y));
	if (!hit)
		return 0;
	if ((result = closeButton.handle(event)) != 0) {
		switch (result) {
		case BUTTON_CLICKED:
			return GUMP_CLOSE;
		default:
			return result;
		}
	}
	int8_t i;
	for (i = 0; i < PARTY_SIZE; i++) {
		if (attackModeButtons[i].isVisible()) {
			if ((result = attackModeButtons[i].handle(event)) != 0) {
				int16_t mode;
				if (result == GUMP_HANDLED)
					return result;
				mode = attackModeButtons[i].getFrame();
				if (mode == 4 && protectButtons[i].getFrame()) {
					mode++;
					attackModeButtons[i].setFrame(mode);
				}
				if (i != 0) {
					if (mode == 9)
						attackModeButtons[i].setFrame(0);
				} else {
					switch (mode) {
					case 3:
					case 4:
						attackModeButtons[i].setFrame(5);
						break;
					case 6:
					case 7:
					case 8:
						attackModeButtons[i].setFrame(9);
						break;
					}
				}
				objref member = PartyMembers[i];
				if ((uint16_t)(attackModeButtons[i].getFrame()) != Item_getDefaultAttackMode(member)) {
					Item_setDefaultAttackMode(member, attackModeButtons[i].getFrame());
					if (IsAvatarInCombat()) {
						Item_setAttackMode(member, attackModeButtons[i].getFrame());
						goto handled;
					}
				}
				return GUMP_HANDLED;
			}
		}
		if (protectButtons[i].isVisible()) {
			if ((result = protectButtons[i].handle(event)) != 0) {
				switch (result) {
				case BUTTON_ON:
					if (attackModeButtons[i].getFrame() != 4) {
						objref member = PartyMembers[i];
						if (PARTY_SIZE > 1) {
							int16_t leader = GetGroupLeader(1);
							if (leader != -1) {
								objref npc;
								GetNpcIbo(&npc, leader);
								int16_t index = GetPartyIndex(npc);
								if (index < PARTY_SIZE)
									protectButtons[index].setFrame(0);
							}
							leader = Item_getNpcNumber(&member);
							SetGroupLeader(leader);
						}
						break;
					}
					protectButtons[i].setFrame(0);
					break;
				case BUTTON_OFF:
					ClearGroupLeader(1);
					ReleaseProtectors(1, -1);
					break;
				}
			handled:
				return GUMP_HANDLED;
			}
		}
		if (faces[i].isVisible() && (result = faces[i].handle(event)) != 0) {
			switch (result) {
			case BUTTON_CLICKED:
				return GUMP_SELECT_ITEM;
			case BUTTON_DOUBLE_CLICK:
				owner = objref(PartyMembers[i].off);
				return GUMP_OWNER;
			default:
				return result;
			}
		}
	}
	if (event->action == MOUSE_CLICK)
		return GUMP_MOVE;
	if (event->action == MOUSE_RELEASE)
		return GUMP_NO_DROP;
	return 0;
}

void CombatStatsGump::draw(View *target)
{
	if (isVisible()) {
		int16_t x = bounds.x;
		int16_t y = bounds.y;
		if (extraColumns)
			drawWide(target);
		else
			ShapeManager_draw(&gShapeManager, target, x, y, shape, 0, 0, 0);
		View *saved = CombatTextPrinter.target;
		CombatTextPrinter.target = target;
		int8_t i;
		char text[20];
		int16_t textX = 97;
		int16_t textY = 99;
		sprintf(text, GetGameText(3, 239), Npc_getMagic(&AvatarRef));
		CombatTextPrinter.x = x + textX;
		CombatTextPrinter.y = y + textY;
		CombatTextPrinter.printString(text);
		textY = 112;
		sprintf(text, GetGameText(3, 239), Npc_getMana(&AvatarRef));
		CombatTextPrinter.x = x + textX;
		CombatTextPrinter.y = y + textY;
		CombatTextPrinter.printString(text);
		textX = 68;
		for (i = 0; i < PARTY_SIZE; i++) {
			textX += 29;
			textY = 35;
			sprintf(text, GetGameText(3, 239), GetNpcBufferForIbo(&PartyMembers[i])->combat);
			CombatTextPrinter.x = x + textX;
			CombatTextPrinter.y = y + textY;
			CombatTextPrinter.printString(text);
			textY = 48;
			sprintf(text, GetGameText(3, 239), Item_getHitPoints(&PartyMembers[i]));
			CombatTextPrinter.x = x + textX;
			CombatTextPrinter.y = y + textY;
			CombatTextPrinter.printString(text);
		}
		CombatTextPrinter.target = saved;
	}
}

/* The six-member art twice, the second copy moved right by the extra columns. They meet where
 * the art's columns repeat, so the seam does not show. */
void CombatStatsGump::drawWide(View *target)
{
	int16_t seam = bounds.x + 180;

	ShapeManager_drawColumns(&gShapeManager, target, bounds.x, bounds.y, shape, bounds.x, seam - 1);
	ShapeManager_drawColumns(&gShapeManager, target, bounds.x + extraColumns * 29, bounds.y, shape, seam,
		bounds.x1);
}

/* While dragged, the screen under the whole picture is kept, not just under the art. */
void CombatStatsGump::show(int16_t x, int16_t y)
{
	if (!extraColumns) {
		Gump::show(x, y);
		return;
	}
	lastX = x - hotX;
	lastY = y - hotY;
	gShapeManager.saveUnderShape(&Viewport, saved, lastX, lastY, image, 0);
}

void CombatStatsGump::hide()
{
	if (!extraColumns)
		Gump::hide();
	else
		gShapeManager.restoreUnderShape(&Viewport, saved, lastX, lastY, image, 0);
}

void CombatStatsGump::moveTo(int16_t x, int16_t y)
{
	int8_t i;

	bounds.moveTo(x, y);
	closeButton.moveTo(x + 23, y + 86);
	for (i = 0; i < 8; i++) {
		int16_t dy = 15;
		if (i == 0 && !Npc_isMale(&AvatarRef))
			dy -= 2;
		faces[i].moveTo(x + i * 29 + 97, y + dy);
		attackModeButtons[i].moveTo(x + i * 29 + 111, y + 73);
		protectButtons[i].moveTo(x + i * 29 + 110, y + 87);
	}
}

uint8_t CombatStatsGump::accepts(objref, int16_t, int16_t) { return 0; }

objref CombatStatsGump::selected()
{
	objref result = 0;

	return result;
}

int16_t CombatStatsGump::mouseX() { return clickX; }

int16_t CombatStatsGump::mouseY() { return clickY; }

int16_t CombatStatsGump::dragX() { return dragOffsetX; }

int16_t CombatStatsGump::dragY() { return dragOffsetY; }

void CombatStatsGump::setDragX(int16_t x) { dragOffsetX = x; }

void CombatStatsGump::setDragY(int16_t y) { dragOffsetY = y; }

void CombatStatsGump::refresh(int8_t force)
{
	if (dirty || force)
		update();
}

uint8_t CombatStatsGump::findPosition(objref, int16_t *, int16_t *) { return 0; }

extern "C" void ResetCombg_ovGlobals(void)
{
	memset((void *)&CombatTextPrinter, 0, sizeof(CombatTextPrinter));
}

extern "C" void ConstructCombg_ovGlobals(void)
{
	new (&CombatTextPrinter) ProportionalTextPrinter();
}
