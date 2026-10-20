#include "u7port.h"
#include <string.h>
#include "rules.h"
#include "itemrec.h"
#include "dollart.h"
#include "dollpics.h"
#include "dollheads.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Serpent Isle's picture for each shape, in PAPERDOL.VGA; frame -1 is any frame. */
static const struct { int16_t type; int8_t frame; uint8_t picture; } DollPictures[] = {
	{ 583, -1, 9 }, { 403, -1, 28 }, { 227, 0, 28 }, { 227, 1, 8 }, { 227, 2, 61 }, { 227, 3, 116 },
	{ 227, 4, 164 }, { 383, 0, 74 }, { 383, 1, 162 }, { 539, -1, 24 }, { 541, -1, 30 },
	{ 542, 0, 31 }, { 542, 1, 88 }, { 542, 2, 114 }, { 543, -1, 20 }, { 545, -1, 34 },
	{ 486, -1, 18 }, { 569, -1, 57 }, { 570, -1, 92 }, { 571, -1, 22 }, { 572, -1, 117 },
	{ 638, -1, 94 }, { 573, -1, 85 }, { 419, -1, 140 }, { 490, -1, 141 }, { 729, -1, 142 },
	{ 836, -1, 1 }, { 574, -1, 60 }, { 575, -1, 26 }, { 576, -1, 87 }, { 578, -1, 104 },
	{ 579, -1, 47 }, { 580, -1, 25 }, { 835, -1, 70 }, { 584, -1, 54 }, { 587, 0, 58 },
	{ 587, 1, 73 }, { 587, 2, 86 }, { 587, 3, 46 }, { 587, 4, 150 }, { 587, 5, 151 }, { 587, 6, 106 },
	{ 609, -1, 37 }, { 663, -1, 75 }, { 666, -1, 2 }, { 677, 0, 152 }, { 677, 1, 153 },
	{ 686, -1, 72 }, { 1004, 0, 59 }, { 1004, 1, 129 }, { 1004, 2, 33 }, { 1004, 3, 165 },
	{ 1004, 4, 129 }, { 586, -1, 19 }, { 585, -1, 38 }, { 640, -1, 96 }, { 241, -1, 105 },
	{ 698, -1, 98 }, { 701, -1, 63 }, { 595, -1, 113 }, { 722, -1, 3 }, { 723, -1, 132 },
	{ 474, -1, 102 }, { 547, -1, 76 }, { 520, -1, 161 }, { 231, -1, 67 }, { 806, -1, 139 },
	{ 942, -1, 138 }, { 535, -1, 143 }, { 926, -1, 144 }, { 549, -1, 62 }, { 551, -1, 41 },
	{ 552, -1, 77 }, { 553, -1, 43 }, { 554, -1, 21 }, { 556, -1, 68 }, { 557, -1, 53 },
	{ 558, -1, 65 }, { 591, -1, 64 }, { 594, -1, 35 }, { 563, -1, 12 }, { 564, -1, 36 },
	{ 567, -1, 110 }, { 568, -1, 101 }, { 589, -1, 84 }, { 590, -1, 29 }, { 592, -1, 103 },
	{ 593, -1, 111 }, { 597, -1, 17 }, { 711, -1, 160 }, { 598, -1, 32 }, { 599, -1, 107 },
	{ 600, -1, 51 }, { 601, -1, 5 }, { 602, -1, 108 }, { 603, -1, 49 }, { 604, -1, 48 },
	{ 605, -1, 16 }, { 606, -1, 69 }, { 608, -1, 109 }, { 618, -1, 93 }, { 620, -1, 91 },
	{ 622, -1, 115 }, { 623, -1, 50 }, { 508, -1, 163 }, { 624, -1, 83 }, { 625, -1, 100 },
	{ 626, -1, 52 }, { 629, -1, 118 }, { 417, -1, 133 }, { 856, -1, 45 }, { 630, -1, 42 },
	{ 662, -1, 44 }, { 659, -1, 66 }, { 596, -1, 81 }, { 792, -1, 78 }, { 771, -1, 134 },
	{ 636, -1, 95 }, { 637, -1, 97 }, { 710, -1, 97 }, { 994, -1, 112 }, { 296, 0, 120 },
	{ 296, 1, 122 }, { 296, 2, 148 }, { 296, 3, 148 }, { 887, 0, 147 }, { 887, 1, 149 },
	{ 802, -1, 89 }, { 801, -1, 6 }, { 955, -1, 0 }, { 635, -1, 39 }, { 761, -1, 15 },
	{ 400, -1, 135 }, { 402, -1, 135 }, { 414, -1, 135 }, { 762, -1, 135 }, { 996, -1, 166 },
	{ 1001, -1, 167 }, { 990, -1, 168 }, { 1013, -1, 169 },
};

/* The Serpent Isle item each Black Gate item looks like; frame -1 is any frame, look frame -1
 * the item's own. Items missing here and from BgOwnPictures are drawn as themselves. */
static const struct { int16_t type; int8_t frame; int16_t look; int8_t lookFrame; } BgDollLooks[] = {
	/* the same item in both games */
	{ 383, -1, 383, 0 }, { 417, -1, 417, -1 }, { 474, -1, 474, -1 }, { 539, -1, 539, -1 },
	{ 541, -1, 541, -1 }, { 542, -1, 542, 0 }, { 543, -1, 543, -1 }, { 545, -1, 545, -1 },
	{ 547, -1, 547, -1 }, { 549, -1, 549, -1 }, { 551, -1, 551, -1 }, { 552, -1, 552, -1 },
	{ 553, -1, 553, -1 }, { 554, -1, 554, -1 }, { 556, -1, 556, -1 }, { 557, -1, 557, -1 },
	{ 558, -1, 558, -1 }, { 563, -1, 563, -1 }, { 564, -1, 564, -1 }, { 567, -1, 567, -1 },
	{ 568, -1, 568, -1 }, { 569, -1, 569, -1 }, { 570, -1, 570, -1 }, { 571, -1, 571, -1 },
	{ 572, -1, 572, -1 }, { 573, -1, 573, -1 }, { 574, -1, 574, -1 }, { 575, -1, 575, -1 },
	{ 576, -1, 576, -1 }, { 578, -1, 578, -1 }, { 579, -1, 579, -1 }, { 580, -1, 580, -1 },
	{ 583, -1, 583, -1 }, { 587, -1, 587, -1 }, { 589, -1, 589, -1 }, { 590, -1, 590, -1 },
	{ 592, -1, 592, -1 }, { 593, -1, 593, -1 }, { 594, -1, 594, -1 }, { 595, -1, 595, -1 },
	{ 596, -1, 596, -1 }, { 597, -1, 597, -1 }, { 598, -1, 598, -1 }, { 599, -1, 599, -1 },
	{ 600, -1, 600, -1 }, { 601, -1, 601, -1 }, { 602, -1, 602, -1 }, { 603, -1, 603, -1 },
	{ 604, -1, 604, -1 }, { 605, -1, 605, -1 }, { 606, -1, 606, -1 }, { 608, -1, 608, -1 },
	{ 609, -1, 609, -1 }, { 618, -1, 618, -1 }, { 620, -1, 620, -1 }, { 622, -1, 622, -1 },
	{ 623, -1, 623, -1 }, { 624, -1, 624, -1 }, { 625, -1, 625, -1 }, { 626, -1, 626, -1 },
	{ 629, -1, 629, -1 }, { 630, -1, 630, -1 }, { 636, -1, 636, -1 }, { 637, -1, 637, -1 },
	{ 659, -1, 659, -1 }, { 662, -1, 662, -1 }, { 663, -1, 663, -1 }, { 666, -1, 666, -1 },
	{ 686, -1, 686, -1 }, { 698, -1, 698, -1 }, { 701, -1, 701, -1 }, { 722, -1, 722, -1 },
	{ 723, -1, 723, -1 }, { 761, -1, 761, -1 }, { 771, -1, 771, -1 }, { 792, -1, 792, -1 },
	{ 801, -1, 801, -1 }, { 802, -1, 802, -1 }, { 835, -1, 835, -1 }, { 836, -1, 836, -1 },
	{ 994, -1, 994, -1 }, { 1004, -1, 1004, 0 },
	/* brown cloak */
	{ 285, 2, 227, 1 },
	/* rings */
	{ 295, -1, 296, 0 }, { 296, -1, 296, 0 }, { 297, -1, 296, 0 }, { 298, -1, 296, 1 },
	{ 640, 0, 296, 0 }, { 640, 1, 887, 0 }, { 759, 0, 296, 0 }, { 759, 1, 296, 2 },
	/* amulets: Black Gate has twelve, Serpent Isle ten */
	{ 955, 0, 955, 0 }, { 955, 1, 955, 5 }, { 955, 2, 955, 5 }, { 955, 3, 955, 3 },
	{ 955, 4, 955, 3 }, { 955, 5, 955, 9 }, { 955, 6, 955, 6 }, { 955, 7, 955, 2 },
	{ 955, 8, 955, 2 }, { 955, 9, 955, 8 }, { 955, 10, 955, 4 }, { 955, 11, 955, 2 },
	/* look-alikes: greaves, swamp boots */
	{ 353, -1, 576, -1 }, { 588, -1, 587, 6 },
	/* Black Sword, Magebane, magic boomerang, bolts, love arrows, musket ammunition */
	{ 707, -1, 806, -1 }, { 559, -1, 231, -1 }, { 550, -1, 605, -1 }, { 948, -1, 723, -1 },
	{ 560, -1, 722, -1 }, { 581, -1, 722, -1 },
};

/* Black Gate items drawn with our own pictures, and the Serpent Isle item whose arm pose and
 * slot rules they follow (-1: none). */
static const struct { int16_t type; int8_t frame; uint8_t picture; int16_t look; } BgOwnPictures[] = {
	/* the grey and green cloaks (the brown one is Serpent Isle's); top, Avatar costume and
	 * kidney belt, body armour */
	{ 285, 0, DOLLPIC_CLOAK_GREY, -1 }, { 285, 1, DOLLPIC_CLOAK_GREEN, -1 },
	{ 249, 0, DOLLPIC_TOP_GREEN, -1 }, { 249, 1, DOLLPIC_TOP_ORANGE, -1 },
	{ 249, 2, DOLLPIC_TOP_BLUE, -1 }, { 838, -1, DOLLPIC_AVATAR_COSTUME, -1 },
	{ 584, -1, DOLLPIC_KIDNEY_BELT, 584 },
	/* weapons: cleaver, musket, starburst, flaming oil, hawk; great dagger, knife, main gauche,
	 * custom sword (held as dagger and sword); Fellowship staff, triple crossbow, Death Scythe,
	 * Hoe of Destruction (held as staff, crossbow, scythe, hoe); the musket and lute are held as
	 * a bow */
	{ 614, -1, DOLLPIC_CLEAVER, -1 }, { 278, -1, DOLLPIC_MUSKET, 711 },
	{ 565, -1, DOLLPIC_STARBURST, -1 }, { 782, -1, DOLLPIC_FLAMING_OIL, -1 },
	{ 555, -1, DOLLPIC_HAWK, -1 }, { 561, -1, DOLLPIC_GREAT_DAGGER, 594 },
	{ 615, -1, DOLLPIC_KNIFE, 594 }, { 591, -1, DOLLPIC_MAIN_GAUCHE, 594 },
	{ 635, -1, DOLLPIC_CUSTOM_SWORD, 599 }, { 885, -1, DOLLPIC_FELLOWSHIP_STAFF, 241 },
	{ 647, -1, DOLLPIC_TRIPLE_CROSSBOW, 598 }, { 562, -1, DOLLPIC_DEATH_SCYTHE, 618 },
	{ 548, -1, DOLLPIC_HOE_OF_DESTRUCTION, 626 }, { 692, -1, DOLLPIC_LUTE, 711 },
	/* pants, shoes, hood and Caddellite helmet (worn as Serpent Isle's pants, boots, helm) */
	{ 738, -1, DOLLPIC_PANTS, 574 }, { 585, -1, DOLLPIC_SHOES, 587 },
	{ 444, -1, DOLLPIC_HOOD, 1004 }, { 638, -1, DOLLPIC_CADDELLITE_HELM, 1004 },
	/* neck: gorget, magic gorget, leather collar */
	{ 586, -1, DOLLPIC_GORGET, -1 }, { 843, -1, DOLLPIC_MAGIC_GORGET, -1 },
	{ 582, -1, DOLLPIC_LEATHER_COLLAR, -1 },
};

/* Our pictures that hold a female and a male frame for each of the item's frames. */
static const int16_t BgPairedPictures[] = { 738, 444, 586, 843, 582 };

/* An item takes its look from its own game's tables, whichever game is played. */
static uint8_t IsBgItem(objref item)
{
	return ShapeGame(item.type()) == GAME_BG;
}

/* Black Gate's row for an item in our pictures, or -1. */
static int16_t FindBgOwn(objref item)
{
	uint16_t type = item.type() & 0x3ff;

	for (uint16_t i = 0; i < COUNT(BgOwnPictures); i++)
		if (BgOwnPictures[i].type == type && (BgOwnPictures[i].frame < 0 || BgOwnPictures[i].frame == item.frame()))
			return i;
	return -1;
}

/* Black Gate's row for an item, or -1. */
static int16_t FindBgLook(objref item)
{
	uint16_t type = item.type() & 0x3ff;

	for (uint16_t i = 0; i < COUNT(BgDollLooks); i++)
		if (BgDollLooks[i].type == type && (BgDollLooks[i].frame < 0 || BgDollLooks[i].frame == item.frame()))
			return i;
	return -1;
}

int16_t DollType(objref item)
{
	if (!IsBgItem(item))
		return item.type() & 0x3ff;
	int16_t i = FindBgOwn(item);
	if (i >= 0)
		return BgOwnPictures[i].look;
	i = FindBgLook(item);
	return i < 0 ? -1 : BgDollLooks[i].look;
}

uint16_t DollFrame(objref item)
{
	if (!IsBgItem(item))
		return item.frame();
	int16_t i = FindBgLook(item);
	return i < 0 || BgDollLooks[i].lookFrame < 0 ? item.frame() : BgDollLooks[i].lookFrame;
}

uint8_t FindDollPicture(int16_t type, uint16_t frame, int16_t *picture)
{
	uint8_t result = DOLL_NO_PICTURE;

	for (uint16_t i = 0; i < COUNT(DollPictures); i++) {
		if (DollPictures[i].type != type)
			continue;
		if (DollPictures[i].frame < 0 || DollPictures[i].frame == frame) {
			*picture = DollPictures[i].picture;
			return DOLL_PICTURE;
		}
		result = DOLL_OTHER_FRAME;
	}
	return result;
}

static uint32_t ReadLong(const uint8_t *p)
{
	return p[0] | p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static void WriteLong(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
	p[2] = (uint8_t) (v >> 16);
	p[3] = (uint8_t) (v >> 24);
}

/* Moves the colours of one frame's runs through map. */
static void RecolourFrame(uint8_t *frame, const uint8_t map[256])
{
	uint8_t *p = frame + 8;

	for (;;) {
		uint16_t scan = p[0] | p[1] << 8;
		uint16_t length = scan >> 1;

		p += 6;
		if (scan == 0)
			return;
		if (!(scan & 1)) {
			for (; length > 0; length--, p++)
				*p = map[*p];
			continue;
		}
		/* runs: a byte of length and kind, then one colour repeated or that many colours */
		while (length > 0) {
			uint8_t run = *p >> 1;
			uint8_t repeat = *p++ & 1;

			for (uint8_t i = 0; i < (repeat ? 1 : run); i++, p++)
				*p = map[*p];
			length -= run;
		}
	}
}

int32_t MakeDerivedDollPicture(const DerivedDollPicture *how, const uint8_t *in, int32_t size, uint8_t *out)
{
	static const uint8_t emptyFrame[10] = { 0 };
	uint8_t map[256];
	uint32_t count = (ReadLong(in + 4) - 4) / 4;
	uint32_t at = 4 + 4 * count;

	for (int16_t i = 0; i < 256; i++)
		map[i] = (uint8_t) i;
	for (uint8_t m = 0; m < how->moveCount; m++)
		for (int16_t c = how->moves[m].first; c <= how->moves[m].last; c++)
			map[c] = (uint8_t) (how->moves[m].to + c - how->moves[m].first);
	for (uint32_t f = 0; f < count; f++) {
		uint32_t start = ReadLong(in + 4 + 4 * f);
		uint32_t end = f + 1 < count ? ReadLong(in + 8 + 4 * f) : (uint32_t) size;

		WriteLong(out + 4 + 4 * f, at);
		if (how->emptyFrames & (UINT32_C(1) << f)) {
			memcpy(out + at, emptyFrame, sizeof emptyFrame);
			at += sizeof emptyFrame;
			continue;
		}
		memcpy(out + at, in + start, end - start);
		RecolourFrame(out + at, map);
		at += end - start;
	}
	WriteLong(out, at);
	return (int32_t) at;
}

int16_t DollPairFrame(objref item)
{
	if (!IsBgItem(item) || FindBgOwn(item) < 0)
		return -1;
	for (uint16_t i = 0; i < COUNT(BgPairedPictures); i++)
		if (BgPairedPictures[i] == (item.type() & 0x3ff))
			return 2 * item.frame();
	return -1;
}

uint8_t FindItemPicture(objref item, uint8_t slot, int16_t *picture)
{
	int16_t i = IsBgItem(item) ? FindBgOwn(item) : -1;

	/* the kidney belt is body armour in Black Gate; worn on the belt it takes Serpent Isle's
	 * kidney belt picture, drawn for that slot */
	if (i >= 0 && slot == 11 && item.type() == 584)
		i = -1;
	if (i < 0)
		return FindDollPicture(DollType(item), DollFrame(item), picture);
	*picture = BgOwnPictures[i].picture;
	return DOLL_PICTURE;
}

int16_t DollNpcType(NPCRef npc)
{
	int16_t type = npc.type();

	if (!PlayingBlackGate())
		return type;
	/* the Avatar (male, female), Iolo, Shamino, Dupre */
	switch (type) {
	case 721:
	case 989:
	case 465:
	case 487:
	case 488:
		return type;
	}
	return -1;
}

int16_t DollNpcNumber(NPCRef npc)
{
	return PlayingBlackGate() ? -1 : Item_getNpcNumber(&npc);
}

/* A Black Gate companion's own head picture, or -1. */
int16_t DollOwnHead(NPCRef npc)
{
	return PlayingBlackGate() ? BgCompanionHead(Item_getNpcNumber(&npc)) : -1;
}

uint8_t DollHasHead(NPCRef npc)
{
	return !PlayingBlackGate() || DollNpcType(npc) >= 0 || DollOwnHead(npc) >= 0;
}

/* Black Gate's stat bytes are whole numbers with no skin or Petra bits. */
uint8_t DollSkinColor(objref *ref)
{
	return PlayingBlackGate() ? 0 : Npc_getSkinColor(ref);
}

uint8_t DollPetraFlag(objref *ref)
{
	return PlayingBlackGate() ? 0 : Npc_hasPetraFlag(ref);
}

int16_t DollFace(NPCRef npc)
{
	if (npc.isAvatar())
		return Npc_isMale(&npc) ? 128 : 4;
	switch (npc.type()) {
	case 465: return 125;
	case 487: return 126;
	case 488: return 124;
	}
	return DollOwnHead(npc);
}
