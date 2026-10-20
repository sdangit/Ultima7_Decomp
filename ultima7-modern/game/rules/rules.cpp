#include "u7port.h"
#include "plat.h"
#include "rules.h"
#include "keyring.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

uint8_t TranslatesSigns(uint8_t readFlag)
{
	return readFlag || plat_setting("runes");
}

/* tests that compare with the original ports set full_rate_animation */
uint8_t AnimatesEveryOtherDraw(void)
{
	return !plat_setting("full_rate_animation");
}

/* tests that compare with the original ports set original_lighting */
uint8_t KeepsOriginalLighting(void)
{
	return plat_setting("original_lighting") != 0;
}

static uint8_t Contains(const int16_t *list, uint16_t count, int16_t value)
{
	for (uint16_t i = 0; i < count; i++)
		if (list[i] == value)
			return 1;
	return 0;
}

/* Black Gate's keys stay off the ring when its people give, take back or ask after them: Garritt's
 * (224), Sintag's (234), Goth's (248), Ian's (249), the baths' (251), Spark's father's chest (253),
 * Alagner's storeroom (254) and the inns' (255); the two jail doors whose locking Black Gate's
 * own key records for its jailers (228, 247); and the qualities Serpent Isle's key code, which
 * opens locks for the ring, ties to its own people (117, 186). */
uint8_t KeyRingRefuses(uint8_t frame, uint8_t quality)
{
	static const int16_t BgQuestKeys[] = {
		117, 186, 224, 228, 234, 247, 248, 249, 251, 253, 254, 255
	};

	if (!PlayingBlackGate())
		return KeyRing.acceptsKey(frame, quality);
	return Contains(BgQuestKeys, COUNT(BgQuestKeys), quality);
}

uint8_t SameShapeIn(enum GameType game, uint16_t n, enum GameType other)
{
	/* doors, locked chest, key, chest: the same numbers in both games */
	static const int16_t BgSiShapes[] = { 270, 376, 432, 433, 522, 641, 800 };

	if (game > GAME_SI || other > GAME_SI || game == other)
		return 0;
	return Contains(BgSiShapes, COUNT(BgSiShapes), n);
}

/* Serpent Isle's key ring in the Avatar's pack; its ring of reagents in the desk of the hidden
 * cave at 117 S 44 W; its Helm of Light in a chest in Serpent's Hold's hall; its everlasting
 * goblet in the pirates' treasure chest, the hoard the parrot places at 169 S 28 E. */
static const StartItem BgStartItems[] = {
	{GAME_SI, 485, 0, 0, START_IN_PACK, 0, 0, 0},
	{GAME_SI, 296, 3, 0, START_IN_CONTAINER, 484, 2313, 283},
	{GAME_SI, 1013, 0, 0, START_IN_CONTAINER, 1449, 2722, 522},
	{GAME_SI, 616, 20, 0, START_IN_CONTAINER, 1205, 2804, 522},
};

const StartItem *StartItems(uint16_t *count)
{
	*count = 0;
	/* tests that compare saves byte for byte set no_start_items */
	if (!PlayingBlackGate() || plat_setting("no_start_items"))
		return 0;
	*count = COUNT(BgStartItems);
	return BgStartItems;
}

/* Black Gate's READY.DAT codes as Serpent Isle's: 4 neck -> 3, 5 body -> 12, 6 ring -> 8,
 * 8 ammunition -> 10, 9 head -> 4, 10 legs -> 14, 11 feet -> 13, 21 gloves -> 5. Its triple
 * bolts (15) go with the ammunition; tongs (18) go in a hand. Spells (0), one hand (1) and two
 * hands (20) mean the same in both. Its cloak, body armour there, goes in the cape slot (2). */
uint8_t SiReadyCode(int16_t type, uint8_t code)
{
	if (IsBgCloak(type))
		return 2;
	switch (code) {
	case 4: return 3;
	case 5: return 12;
	case 6: return 8;
	case 8: return 10;
	case 9: return 4;
	case 10: return 14;
	case 11: return 13;
	case 15: return 10;
	case 18: return 1;
	case 21: return 5;
	}
	return code;
}

uint8_t IsBgCloak(int16_t type)
{
	return IsGameShape(type, GAME_BG, 285);
}

/* Serpent Isle's slot for each Black Gate slot: back, two hands, belt, neck, body, two rings,
 * ammunition, head, legs, feet. */
const uint8_t BgSlotToSi[BG_SLOT_COUNT] = { 15, 1, 0, 11, 3, 12, 7, 8, 10, 4, 14, 13 };

/* What the belt, back and the two spare back slots take. */
static const int16_t SiBeltShapes[] = {
	231, 474, 508, 520, 535, 547, 549, 551, 552, 563, 564, 567, 584, 590, 593, 594, 595, 596,
	598, 599, 604, 605, 608, 622, 623, 629, 630, 636, 637, 659, 698, 710, 771, 792, 802, 926,
	990, 994, 996,
};
static const int16_t SiBackShapes[] = { 583, 801 };
static const int16_t SiBackShieldShapes[] = { 490, 543, 545, 572, 578, 585, 586, 609, 663, 729 };
static const int16_t SiBackWeaponShapes[] = {
	241, 553, 557, 583, 589, 592, 597, 600, 601, 602, 603, 606, 618, 620, 624, 625, 626, 640,
	662, 711, 806, 942,
};

/* Serpent Isle's lists in Black Gate's numbers, with Black Gate's own one-handed weapons on the
 * belt (Magebane, great dagger, main gauche, cleaver, knife, magic boomerang, custom sword), the
 * triple crossbow there too like the crossbow, and two-handed ones on the back (musket, Death
 * Scythe, Fellowship staff, Hoe of Destruction). */
static const int16_t BgBeltShapes[] = {
	474, 547, 549, 550, 551, 552, 559, 561, 563, 564, 567, 584, 590, 591, 593, 594, 595, 596,
	598, 599, 604, 605, 608, 614, 615, 622, 623, 629, 630, 635, 636, 637, 647, 659, 698, 771,
	792, 802, 994,
};
static const int16_t BgBackShapes[] = { 583, 801 };
static const int16_t BgBackShieldShapes[] = { 543, 545, 572, 578, 609, 663 };
static const int16_t BgBackWeaponShapes[] = {
	278, 548, 553, 557, 562, 583, 589, 592, 597, 600, 601, 602, 603, 606, 618, 620, 624, 625, 626,
	662, 707, 885,
};

/* Each game's lists hold its own numbers. */
uint8_t SlotTakesShape(uint8_t slot, int16_t type)
{
	uint8_t bg = ShapeGame(type) == GAME_BG;

	type &= 0x3ff;
	switch (slot) {
	case 11:
		return bg ? Contains(BgBeltShapes, COUNT(BgBeltShapes), type)
			: Contains(SiBeltShapes, COUNT(SiBeltShapes), type);
	case 15:
		return bg ? Contains(BgBackShapes, COUNT(BgBackShapes), type)
			: Contains(SiBackShapes, COUNT(SiBackShapes), type);
	case 16:
		return bg ? Contains(BgBackShieldShapes, COUNT(BgBackShieldShapes), type)
			: Contains(SiBackShieldShapes, COUNT(SiBackShieldShapes), type);
	case 17:
		return bg ? Contains(BgBackWeaponShapes, COUNT(BgBackWeaponShapes), type)
			: Contains(SiBackWeaponShapes, COUNT(SiBackWeaponShapes), type);
	}
	return 0;
}

/* Lit weapons that may be worn away from the hands. */
uint8_t LitShapeFitsAnySlot(int16_t type)
{
	static const int16_t si[] = { 551, 553, 549, 926, 1013 };
	static const int16_t bg[] = { 551, 553, 549 };

	if (ShapeGame(type) == GAME_BG)
		return Contains(bg, COUNT(bg), type & 0x3ff);
	return Contains(si, COUNT(si), type & 0x3ff);
}

/* Items whose own usecode runs when they are put on or taken off. */
uint8_t ShapeHasEquipScript(int16_t type)
{
	static const int16_t si[] = { 209, 296, 336, 338, 595, 701, 806, 990, 996, 1001, 1013 };
	static const int16_t bg[] = { 296, 298, 336, 338, 595, 701 };

	/* by the item's own game */
	if (ShapeGame(type) == GAME_BG)
		return Contains(bg, COUNT(bg), type & 0x3ff);
	return Contains(si, COUNT(si), type & 0x3ff);
}

uint8_t IsSealedContainer(int16_t type)
{
	return type == 522 || (PlayingBlackGate() && type == 798);
}

uint8_t ContainerRefuses(int16_t container, int16_t item)
{
	if (IsSealedContainer(container))
		return 1;
	if (PlayingBlackGate())
		return 0;
	return container == 914 || (container == 555 && item != 559);
}

uint8_t CorpseRefusesItems(int16_t type)
{
	static const int16_t si[] = { 762, 892, 402, 778, 414, 400 };

	return !PlayingBlackGate() && Contains(si, COUNT(si), type);
}

/* Shields, which go to the shield hand first. */
uint8_t IsShieldShape(int16_t type)
{
	static const int16_t si[] = { 490, 572, 578, 585, 586, 609, 663, 543, 729 };
	static const int16_t bg[] = { 543, 545, 572, 578, 609, 663 };

	if (PlayingBlackGate())
		return Contains(bg, COUNT(bg), type);
	return Contains(si, COUNT(si), type);
}

/* Black Gate's flaming oil, firedoom staff and fire sword; Serpent Isle's lit weapons. */
uint8_t GlowsLikeTorch(int16_t type)
{
	static const int16_t si[] = { 553, 551, 926, 549, 1013 };
	static const int16_t bg[] = { 782, 553, 551 };

	/* by the item's own game */
	if (ShapeGame(type) == GAME_BG)
		return Contains(bg, COUNT(bg), type & 0x3ff);
	return Contains(si, COUNT(si), type & 0x3ff);
}

/* Each text's number in section 2, Black Gate's then Serpent Isle's. */
static const int16_t TextNumbers[GAME_TEXT_COUNT][2] = {
	{ 68, 80 },     /* TEXT_AFTER_SAVING */
	{ 69, 81 },     /* TEXT_CIRCLE */
	{ 69, 82 },     /* TEXT_CIRCLE_NAMES; Black Gate's Linear circle has no name */
};

int16_t TextNumber(enum GameText text)
{
	return TextNumbers[text][PlayingBlackGate() ? 0 : 1];
}

/* Black Gate's gump for each Serpent Isle gump number. Its own gumps (direction arrows, the
 * inventory backgrounds) take numbers Serpent Isle leaves empty. */
static const int8_t BgGumpRecords[100] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
	22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 55, 56, 69, 70, 71,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	18, 19, 20, 21, 23, -1, 57, 58, 59, 60, 61, 62, 63, 65, 66, 67, -1, 68,
	-1, -1, -1, -1, -1, -1, -1,
};

int16_t GumpRecord(int16_t gump)
{
	if (!PlayingBlackGate())
		return gump;
	if (gump < 0 || gump >= (int16_t)COUNT(BgGumpRecords))
		return -1;
	return BgGumpRecords[gump];
}

int16_t BgGump(int16_t record)
{
	for (int16_t i = 0; i < (int16_t)COUNT(BgGumpRecords); i++)
		if (BgGumpRecords[i] == record)
			return i;
	return -1;
}

uint8_t IsSerpentIsleGump(int16_t gump)
{
	/* 58-63: the combat status window for one to six members */
	return PlayingBlackGate() && (gump == 54 || gump == 55 || gump == 91 || gump >= 58 && gump <= 63);
}

uint8_t BgShapeOpens(int16_t type)
{
	static const int16_t shapes[] = {
		283, 400, 405, 406, 407, 414, 416, 507, 679, 761, 762, 778, 799, 800, 801, 802, 803, 804,
		819, 892,
	};

	return Contains(shapes, COUNT(shapes), type);
}

/* Black Gate's mages (154, 445, 446), ghosts (299, 317), liches (354, 519), dragon (504),
 * gazer (511), Kissme (382) and wisp (534). Serpent Isle has Aram Dol (981) and the gazer turn
 * invisible, and none teleport or summon. */
uint8_t TurnsInvisible(int16_t type)
{
	static const int16_t si[] = { 981, 511 };
	static const int16_t bg[] = { 445, 446, 154, 317, 299, 519, 504, 511, 354 };

	if (PlayingBlackGate())
		return Contains(bg, COUNT(bg), type);
	return Contains(si, COUNT(si), type);
}

uint8_t TeleportsInCombat(int16_t type)
{
	static const int16_t bg[] = { 445, 446, 154, 317, 299, 382, 519, 534, 354 };

	return PlayingBlackGate() && Contains(bg, COUNT(bg), type);
}

uint8_t SummonsInCombat(int16_t type)
{
	static const int16_t bg[] = { 445, 446, 154, 519, 354 };

	return PlayingBlackGate() && Contains(bg, COUNT(bg), type);
}

/* Each sound's number in Black Gate, then in Serpent Isle. */
static const int16_t SoundNumbers[GAME_SOUND_COUNT][2] = {
	{ 10, 1 },      /* MUSIC_BATTLE */
	{ 11, 2 },      /* MUSIC_GUARDS */
	{ 16, 7 },      /* MUSIC_RETREAT */
	{ 17, 8 },      /* MUSIC_AVATAR_DIED */
	{ 9, 0 },       /* MUSIC_HARD_WON */
	{ 15, 6 },      /* MUSIC_VICTORY */
	{ 18, 9 },      /* MUSIC_FIGHT_OVER */
	{ 52, 42 },     /* MUSIC_DUNGEON */
	{ 6, 67 },      /* MUSIC_OUTDOORS */
	{ 5, 69 },      /* MUSIC_RAIN */
	{ 4, 68 },      /* MUSIC_SNOW */
	{ 5, 61 },      /* SFX_ARMOUR_HIT */
	{ 1, 80 },      /* SFX_MISSILE_HIT */
	{ 37, 95 },     /* SFX_GLASS_SWORD_HIT */
	{ 4, 125 },     /* SFX_WEAPON_HIT */
	{ 44, 1 },      /* SFX_TURN_INVISIBLE */
	{ 43, 2 },      /* SFX_TELEPORT */
	{ 60, 39 },     /* SFX_SKELETONS_RISE */
	{ 67, 130 },    /* SFX_SUMMON */
	{ 76, 43 },     /* SFX_NO_CAN_DO */
	{ 60, 62 },     /* SFX_QUAKE */
	{ 62, 116 },    /* SFX_THUNDER */
	{ 18, 118 },    /* SFX_ITEM_SET_DOWN */
	{ 77, -1 },     /* SFX_LOADING_SCREEN */
	{ 14, 94 },     /* SFX_WINDOW_OPEN */
	{ 73, 134 },    /* SFX_BUTTON_CLICK */
	{ 74, 134 },    /* SFX_BUTTON_PRESS */
	{ 2, 9 },       /* SFX_FENCING_SWING */
	{ 47, 7 },      /* SFX_BELLOWS */
	{ 45, 49 },     /* SFX_HAMMER */
	{ 46, 33 },     /* SFX_SPLASH */
	{ 97, 100 },    /* SFX_SPINNING_WHEEL */
	{ 102, 73 },    /* SFX_LOOM */
	{ 7, 40 },      /* SFX_FIELD */
	{ 30, 32 },     /* SFX_DOOR_OPEN */
	{ 86, 10 },     /* SFX_FALL_ASLEEP */
};

int16_t SoundNumber(enum GameSound sound)
{
	return SoundNumbers[sound][PlayingBlackGate() ? 0 : 1];
}

int16_t QuantityFrameBase(int16_t type)
{
	static const int16_t siMissiles[] = { 417, 554, 556, 558, 568, 591, 722, 723 };
	static const int16_t siOthers[] = { 627, 644, 948, 951, 952 };
	static const int16_t bgMissiles[] = { 417, 554, 556, 558, 560, 568, 722, 723, 948 };
	static const int16_t bgOthers[] = { 581, 627, 644 };
	uint8_t bg = ShapeGame(type) == GAME_BG;

	/* by the item's own game */
	type &= 0x3ff;
	if (bg) {
		if (Contains(bgMissiles, COUNT(bgMissiles), type))
			return 24;
		return Contains(bgOthers, COUNT(bgOthers), type) ? 0 : -1;
	}
	if (Contains(siMissiles, COUNT(siMissiles), type))
		return 24;
	return Contains(siOthers, COUNT(siOthers), type) ? 0 : -1;
}

/* Sheep, chicken, deer, cow and rabbit in both; Black Gate's horse (727), Serpent Isle's boar
 * (329) and leopard (754). */
int16_t MeatFrame(int16_t type)
{
	switch (type) {
	case 970: return 8;
	case 498: return 10;
	case 502: return 14;
	case 500: return 9;
	case 811: return 9;
	case 727: return PlayingBlackGate() ? 23 : -1;
	case 329: return PlayingBlackGate() ? -1 : 11;
	case 754: return PlayingBlackGate() ? -1 : 9;
	}
	return -1;
}

uint8_t FoodFrame(uint8_t n)
{
	static const uint8_t bg[] = { 0, 2, 3, 6, 9, 11, 12, 15, 16, 17, 19, 24, 25, 27, 29, 30, 31 };
	static const uint8_t si[] = { 0, 2, 3, 6, 8, 9, 11, 12, 13, 16, 17, 19, 23, 25, 26, 29, 30 };

	return PlayingBlackGate() ? bg[n] : si[n];
}

uint8_t KitchenFrame(uint8_t n)
{
	static const uint8_t bg[] = { 0, 2, 3, 4, 7, 8 };
	static const uint8_t si[] = { 2, 3, 4, 7, 8 };

	return PlayingBlackGate() ? bg[n] : si[n];
}
