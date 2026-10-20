/* Serpent Isle SI.EXE, overlay segment 299 (file offsets 0x080e10 to 0x0811fb, 1003 bytes).
 * Borland C++ 2.0 -mm -O -P -Y rebuilds it byte for byte as C++.
 */

/* path: usehook.c */
#include "u7port.h"
#include "item.h"
#include "u7event.h"
#include "colbuf.h"
#include "debug.h"
#include "ucstack.h"
#include "routine.h"
#include "sprite.h"
#include "inter.h"
#include "npcref.h"
#include "tools.h"
#include "u7ibuf.h"
#include "memfree.h"
#include "keywords.h"
#include "ucctrl.h"
#include "type.h"
#include "gametype.h"
#include "usehook.h"
#include "ucset.h"
#include "actqueue.h"
#include "script.h"
#include "tools.h"

#define TYPE(rec) Item_shape(RecordItem(rec))
#define TYPE_CLASS(rec) (ShapeInfo(TYPE(rec))->typeClass)
#define CLASS_FLAGS(rec) (ItemTypeClassFlags[TYPE_CLASS(rec)])
#define IS_NPC(rec) ((int8_t) ((CLASS_FLAGS(rec) & CLASS_NPC) != 0))

/* usable numbers: NPCs 0 to 255 have theirs from NPC_USABLES on */
#define NPC_USABLES     0x400
#define NPC_USABLES_END 0x500
#define BOOK_TYPE       642

extern uint8_t SaveLoadActive;
extern uint16_t &LastUsecodeFunction;

int16_t TelekinesisUsable = -1;
int8_t UsableRunning = 0;

/* Run usecode function func for an item, or the item's own function when func is -1. */
int8_t RunUsable(uint8_t event, int16_t item, uint16_t func)
{
	if (SaveLoadActive)
		return 0;

	UsecodeRoutine code;
	ValueStack stack;
	CallStack calls;
	uint16_t mem;
	enum GameType set = FunctionSet(&func);

	mem = NearMemory.getNearFree();
	if (mem < 3500)
		DebugPrintf("\rLow Mem!! %d", mem);
	if (mem < 3000)
		return 0;
	if (func == (uint16_t)-1) {
		if (IS_NPC(objref(item).ptr())) {
			if (event == EVENT_ACTION && IsNpcUnconscious(&objref(item))) {
				return 0;
			}
			if (GameInput.isGumpMode() && !(uint8_t)Item_isAvatar(&objref(item)) && event != 8) {
				return 0;
			}
			if (OinkMode && Item_getNpcNumber(&objref(item)) != 26)
				func = UsecodeHook(HOOK_OINK);
			else {
				if ((uint16_t)Item_getNpcNumber(&objref(item)) >= 256) {
					func = TYPE(objref(item).ptr());
				} else {
					func = Item_getNpcNumber(&objref(item)) + NPC_USABLES;
				}
			}
		} else {
			func = TYPE(objref(item).ptr());
			/* another game's item runs its own usecode */
			if (IsForeignShape(func)) {
				set = ShapeGame(func);
				func &= 0x3ff;
			}
		}
	}
	objref obj = item;
	if (TYPE(obj.ptr()) == BOOK_TYPE) {
		if ((uint8_t)Item_getQuality(&obj) >= 100 && (uint8_t)Item_getQuality(&obj) <= 150)
			func = UsecodeHook(HOOK_BOOK);
		if ((uint8_t)Item_getQuality(&obj) >= 151 && (uint8_t)Item_getQuality(&obj) <= 255)
			func = UsecodeHook(HOOK_LATE_BOOK);
	} else if (TYPE(obj.ptr()) == 705 && (uint8_t)Item_getQuality(&obj) >= 211 && UsecodeHook(HOOK_SERPENT_BOOK))
		func = UsecodeHook(HOOK_SERPENT_BOOK);
	if (func == (uint16_t)TelekinesisUsable)
		event = EVENT_ACTION;
	/* one function at a time; BG turns a second away quietly, SI stops the game */
	if (UsableRunning) {
		if (PlayedGame() == GAME_BG)
			return 0;
		HaltWithMessage(__FILE__, 204, "id = %d, source = %d", func, event);
	}
	UsableRunning = 1;
	LastUsecodeFunction = func;
	SetUsecodeSet(set);
	if (!code.load(func)) {
		SetUsecodeSet(PlayedGame());
		UsableRunning = 0;
		return 0;
	}
	SpriteManager_stopItemSprites(&gSpriteManager, item, 1);
	UC_Interpret(&code, &stack, &calls, ChosenAnswer, item, event);
	SetUsecodeSet(PlayedGame());
	/* after a talk */
	if (func > NPC_USABLES && func < NPC_USABLES_END && event == EVENT_ACTION)
		CheckItemBuffer();
	if (func == (uint16_t)TelekinesisUsable)
		TelekinesisUsable = -1;
	UsableRunning = 0;
	return 1;
}

/* Whether usecode function func exists. */
uint8_t HasUsable(uint16_t func)
{
	UsecodeRoutine code;

	return code.load(func);
}

/* by hook: Black Gate's function, then Serpent Isle's */
static const uint16_t HookFunctions[HOOK_COUNT][2] = {
	{ 0x614, 0x614 },   /* HOOK_SPEECH_TEXT */
	{ 0x60e, 0x60e },   /* HOOK_AVATAR_GONE */
	{ 0x622, 0x622 },   /* HOOK_BED */
	{ 0x625, 0x625 },   /* HOOK_ARREST */
	{ 0x634, 0x634 },   /* HOOK_BARGE */
	{ 0x626, 0x626 },   /* HOOK_BREAK_ITEM */
	{ 0x633, 0x633 },   /* HOOK_THEFT */
	{ 0x63a, 0x63a },   /* HOOK_SCOLD */
	{ 0x640, 0x640 },   /* HOOK_FIRST_SPELL */
	{ 0x638, 0x638 },   /* HOOK_BOOK */
	{ 0x638, 1595 },    /* HOOK_LATE_BOOK */
	{ 0, 1578 },        /* HOOK_SERPENT_BOOK */
	{ 0x63d, 0x63d },   /* HOOK_OINK */
	{ 0x1fa, 0 },       /* HOOK_HOOK_DIES */
	{ 0, 1557 },        /* HOOK_FEED */
	{ 0, 1675 },        /* HOOK_KEY_REFUSED */
	{ 0, 0x68c },       /* HOOK_MOONSHADE */
	{ 0, 1583 },        /* HOOK_MAZE_PATROL */
	{ 0, 2013 },        /* HOOK_MONITOR_HIT */
	{ 0, 2017 }         /* HOOK_SLEEP_ARROW_HIT */
};

uint16_t UsecodeHook(enum UsecodeHook hook)
{
	return HookFunctions[hook][PlayedGame()];
}

/* A game's hook, to run from its own set whichever game is played. */
uint16_t GameUsecodeHook(enum GameType game, enum UsecodeHook hook)
{
	return SetFunction(game, HookFunctions[hook][game]);
}

/* Run a hook for an item from the action queue, where SI's functions get event 2. BG's check for
 * event 1, so they run at once as BG's engine ran them. */
void PostUsecodeHook(uint16_t item, enum UsecodeHook hook)
{
	if (PlayedGame() == GAME_BG) {
		RunUsable(EVENT_ACTION, item, UsecodeHook(hook));
		return;
	}
	uint8_t script[128];
	script[0] = 1;
	AppendScriptByte(script, SCRIPT_NO_HALT);
	AppendScriptByte(script, SCRIPT_USECODE);
	AppendScriptWord(script, UsecodeHook(hook));
	ActionQueue.add(item, (char *)script);
}

/* the event an NPC's own function gets when the NPC comes up to talk */
uint8_t TalkEvent(void)
{
	return PlayedGame() == GAME_BG ? 1 : 9;
}

extern "C" void ResetUsehookGlobals(void)
{
	TelekinesisUsable = -1;
	UsableRunning = 0;
	LastUsecodeFunction = 0;
}
