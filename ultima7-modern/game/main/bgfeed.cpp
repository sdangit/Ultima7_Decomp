/* Black Gate's F key. Its scripts have no routine for it, so this follows their food script: the
 * same nourishment per food frame, eating sound and remarks.
 */

#include "u7port.h"
#include "item.h"
#include "type.h"
#include "npcref.h"
#include "u7npc.h"
#include "search.h"
#include "target.h"
#include "random.h"
#include "sprite.h"
#include "sounds.h"
#include "cast.h"
#include "use.h"
#include "partymov.h"
#include "rules.h"
#include "bgfeed.h"

#define FOOD_TYPE       377
#define CUP_TYPE        628
#define FED_UP          24      /* food level past which nobody eats */
#define EATING_SOUND    91

/* How much each frame of food feeds. */
static const uint8_t FoodValue[32] = {
	4, 6, 2, 5, 3, 3, 1, 12, 24, 16, 24, 24, 4, 8, 16, 6,
	2, 3, 2, 1, 4, 3, 1, 24, 3, 1, 9, 2, 31, 8, 6, 0,
};

/* The text is drawn as given; it must have no @, which drawing would rewrite in place. */
static void Bark(objref who, const char *text)
{
	ClearAllBarks(who.off);
	SpriteManager_barkOnItem(&gSpriteManager, who.off, (char *)text, 0, 15, 0);
}

static uint8_t CanEat(objref who)
{
	return !NPCRef(who).flag(NPC_ASLEEP | NPC_PARALYZED | NPC_DEAD);
}

static uint8_t CanTalk(objref who)
{
	return CanEat(who) && NPCRef(who).buffer()->intelligence >= 10 && Item_getHitPoints(&who) > 0;
}

/* The first food anyone in the party carries that feeds at all. */
static objref FindPartyFood(void)
{
	AreaSearch search;

	for (int8_t i = 0; i < PartySize; i++) {
		for (FindItemInContainer(&search, PartyMembers[i], 0, FOOD_TYPE, -1, 255); search.found(); FindItem(&search)) {
			if (FoodValue[search.current.frame() & 31])
				return search.current;
		}
	}
	return objref(0);
}

/* Goblets that feed without being used up: the endless goblet (a cup in frame 15 of quality 77,
 * from the player's own Black Gate scripts) a draught worth 24, Serpent Isle's goblet (its bottle
 * 616 in frame 20) one at a time, as Serpent Isle's scripts give it. 0 for anything else. */
static uint8_t GobletValue(objref item)
{
	if (PlayingBlackGate() && item.type() == CUP_TYPE && item.frame() == 15 && Item_getQuality(&item) == 77)
		return 24;
	if (IsGameShape(item.type(), GAME_SI, 616) && item.frame() == 20)
		return 1;
	return 0;
}

/* A goblet anyone in the party carries; Serpent Isle drinks from it once the food is gone. */
static objref FindPartyGoblet(void)
{
	AreaSearch search;
	const int16_t types[] = { CUP_TYPE, (int16_t)GameShape(GAME_SI, 616) };

	for (int8_t i = 0; i < PartySize; i++)
		for (uint8_t k = 0; k < 2; k++)
			for (FindItemInContainer(&search, PartyMembers[i], 0, types[k], -1, 255); search.found(); FindItem(&search))
				if (GobletValue(search.current))
					return search.current;
	return objref(0);
}

static void ConsumeOne(objref food)
{
	uint8_t count = Item_getQuantity(&food);

	if (count <= 1)
		Item_delete(&food);
	else
		Item_setQuantity(food, count - 1, 0);
}

/* What someone says on eating food worth value with the given food level. */
static const char *EatingRemark(objref who, uint8_t level, uint8_t value)
{
	int16_t after = level + value;
	int16_t roll = GenerateRandomIntegerInRange(10) + 1;

	if (level <= 4) {
		if (after <= 4)
			return roll < 6 ? "\"I must have more!\"" : "\"More!\"";
		if (after < 10)
			return roll < 6 && !Item_isAvatar(&who) ? "\"May I have some more?\"" : "\"I am still hungry.\"";
		if (after < 20)
			return "\"Ah yes, much better.\"";
		return roll < 6 ? "\"Burp\"" : "\"That hit the spot!\"";
	}
	if (level < 20)
		return after > FED_UP && roll < 3 ? "\"Belch\"" : "\"Ahh, very tasty.\"";
	return roll < 5 ? "\"I'll soon be plump.\"" : 0;
}

void FeedPartyMember(void)
{
	objref who, food;
	Coord x, y;
	int16_t z;

	HavePlayerSelect(&who, &x, &y, &z);
	if (!who.valid() || !who.isNpc() || !Item_isPartyMember(who) || !CanEat(who))
		return;
	for (;;) {
		uint8_t level = NPCRef(who).buffer()->food;
		uint8_t value;
		const char *remark;

		if (level > FED_UP) {
			if (CanTalk(who))
				Bark(who, "\"No, thank thee.\"");
			return;
		}
		food = FindPartyFood();
		if (food.valid())
			value = FoodValue[food.frame() & 31];
		else if ((food = FindPartyGoblet()).valid())
			value = GobletValue(food);
		else
			break;
		PlaySoundAtItem(EATING_SOUND, food);
		if (!GobletValue(food))
			ConsumeOne(food);
		remark = EatingRemark(who, level, value);
		if (remark && CanTalk(who))
			Bark(who, remark);
		Npc_changeFood(&who, value);
	}
	Bark(AvatarRef, "\"No more food.\"");
}
