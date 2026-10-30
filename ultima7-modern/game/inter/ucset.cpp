#include "u7port.h"
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "dosio.h"
#include "easyfile.h"
#include "oops.h"
#include "rules.h"
#include "ucvalue.h"
#include "uclist.h"
#include "uccomm5.h"
#include "uccomm7.h"
#include "uccomm12.h"
#include "script.h"
#include "ucset.h"

/* engine calls whose numbers differ between the sets */
void UC_Post(Value *args, Value *ret);
void UC_PostInFuture(Value *args, Value *ret);
void UC_SetItemShape(Value *args, Value *ret);
void UC_FindNearest(Value *args, Value *ret);
void UC_Sfx(Value *args);
void UC_GetItemShape(Value *args, Value *ret);
void UC_CreateNewObject(Value *args, Value *ret);
void UC_CreateCreature(Value *args, Value *ret);
void UC_CountObjects(Value *args, Value *ret);
void UC_FindObject(Value *args, Value *ret);
void UC_GetContItems(Value *args, Value *ret);
void UC_RemovePartyItems(Value *args, Value *ret);
void UC_AddPartyItems(Value *args, Value *ret);
void UC_GiveToCont(Value *args, Value *ret);
void UC_TakeFromCont(Value *args, Value *ret);
void UC_FindNearbyAvatar(Value *args, Value *ret);
void UC_FindNearby(Value *args, Value *ret);
void UC_IsReadied(Value *args, Value *ret);
void UC_PathRunUsecode(Value *args, Value *ret);
void UC_UnusedItemWalk(Value *args, Value *ret);
void UC_PlaySoundEffect2(Value *args);
void UC_SetPathFailure(Value *args, Value *ret);
void UC_PathfindNPC(Value *args, Value *ret);

/* 0: the played game's set; otherwise another game's, plus one */
static uint8_t RunningSetTag = 0;

/* another game's USECODE, LINKDEP1 and LINKDEP2, whole */
static uint8_t *OtherUsecode[MAX_GAMES];
static uint8_t *OtherLinkdep1[MAX_GAMES];
static uint8_t *OtherLinkdep2[MAX_GAMES];
static uint16_t OtherLinkCount[MAX_GAMES];

#define OTHER_FLAG_COUNT    1024
static uint8_t OtherFlagBits[MAX_GAMES][OTHER_FLAG_COUNT / 8];
static char *const OtherFlagFileNames[MAX_GAMES] = { "FLAGSBG.DAT", "FLAGSSI.DAT", "FLAGSM1.DAT", "FLAGSM2.DAT" };

static uint16_t Word(const uint8_t *p)
{
	return (uint16_t) (p[0] | p[1] << 8);
}

enum GameType UsecodeSet(void)
{
	return RunningSetTag ? (enum GameType) (RunningSetTag - 1) : PlayedGame();
}

uint8_t RunningOtherSet(void)
{
	return RunningSetTag != 0;
}

void SetUsecodeSet(enum GameType set)
{
	RunningSetTag = set == PlayedGame() ? 0 : set + 1;
}

/* The played game's ids stay as they are; the others' carry set + 1 in bits 12-14. */
uint16_t SetFunction(enum GameType set, uint16_t id)
{
	if (set == PlayedGame())
		return id;
	return (uint16_t) (id | (set + 1) << 12);
}

enum GameType FunctionSet(uint16_t *func)
{
	uint16_t tag = (*func >> 12) & 7;

	if (*func == 0xffff || tag == 0)
		return PlayedGame();
	*func &= 0xfff;
	return (enum GameType) (tag - 1);
}

static uint8_t *ReadOtherFile(enum GameType game, const char *name, int32_t *size)
{
	int16_t fd = plat_game_file_open(game, name);
	uint8_t *data;

	if (fd < 0)
		ReportError(0xeb05);
	*size = DosSeek(fd, 0, 2);
	data = (uint8_t *) malloc(*size);
	if (data == 0)
		ReportOutOfNearMemory();
	DosRead(fd, INT32_C(0), *size, data);
	DosClose(fd);
	return data;
}

void LoadOtherUsecode(enum GameType game)
{
	int32_t size;

	OtherUsecode[game] = ReadOtherFile(game, "STATIC\\USECODE", &size);
	OtherLinkdep2[game] = ReadOtherFile(game, "STATIC\\LINKDEP2", &size);
	OtherLinkdep1[game] = ReadOtherFile(game, "STATIC\\LINKDEP1", &size);
	OtherLinkCount[game] = (uint16_t) (size / 4 - 1);
}

/* as LookupLinkdep1 for another set: its first linkdep2 entry, how many, and their size */
uint8_t OtherLinks(enum GameType set, uint16_t id, uint16_t *first, uint16_t *count, uint16_t *offset)
{
	const uint8_t *record;

	if (OtherLinkdep1[set] == 0 || id >= OtherLinkCount[set])
		return 0;
	record = OtherLinkdep1[set] + id * 4;
	*first = Word(record);
	*offset = Word(record + 2);
	*count = Word(record + 4) - *first;
	return 1;
}

/* the function record linkdep2 entry points at: id, size, then the code */
const uint8_t *OtherRecord(enum GameType set, uint16_t entry)
{
	const uint8_t *at = OtherLinkdep2[set] + entry * 4;

	return OtherUsecode[set] + (at[0] | at[1] << 8 | at[2] << 16 | (uint32_t) at[3] << 24);
}

/* Shapes the running set calls the same thing as one of the played game's keep its number. */
uint16_t ShapeToSet(enum GameType set, uint16_t shape)
{
	enum GameType game = ShapeGame(shape);
	uint16_t n = IsForeignShape(shape) ? shape & 0x3ff : shape;

	if (game == set || SameShapeIn(game, n, set))
		return n;
	return (uint16_t) (FIRST_FOREIGN_SHAPE + game * 1024 + n);
}

int16_t ShapeFromSet(enum GameType set, int16_t value)
{
	if (value >= 0 && value < 1024)
		return SameShapeIn(set, value, PlayedGame()) ? value : GameShape(set, value);
	if (value >= FIRST_FOREIGN_SHAPE && value < SHAPE_NUMBERS)
		return GameShape((enum GameType) ((value - FIRST_FOREIGN_SHAPE) >> 10), value & 0x3ff);
	return value;
}

/* Engine calls that take a shape, and which argument (the tables are packed, so two lists). */
static const EngineCall ShapeCalls[] = {
	UC_SetItemShape, UC_FindNearest, UC_CreateNewObject, UC_CreateCreature, UC_CountObjects,
	UC_FindObject, UC_GetContItems, UC_RemovePartyItems, UC_AddPartyItems, UC_GiveToCont,
	UC_TakeFromCont, UC_FindNearbyAvatar, UC_FindNearby, UC_IsReadied,
	(EngineCall) UC_FireProjectile, UC_IsNotBlocked, (EngineCall) UC_Polymorph,
	(EngineCall) UC_RemoveItemsInArea
};
static const uint8_t ShapeArgs[] = { 2, 2, 1, 1, 2, 2, 2, 2, 2, 3, 3, 1, 2, 3, 3, 2, 2, 1 };

/* Engine calls that take a function to run later, and which argument. */
static const EngineCall FunctionCalls[] = {
	UC_PathRunUsecode, UC_SetPathFailure, UC_PathfindNPC, UC_UnusedItemWalk
};
static const uint8_t FunctionArgs[] = { 2, 1, 5, 3 };

static void TranslateNode(Node *node, int16_t value)
{
	if (node->type == NODE_INT)
		*node = (int32_t) value;
}

/* A script's usecode step names its function in the number after it; usecode passes the
 * function as one value above 255. */
static void TagScriptFunctions(enum GameType set, Value *script)
{
	Node *node = 0, *previous = 0;

	while (LinkList_stepForward(script, (Link **)&node)) {
		if (previous && previous->type == NODE_INT && previous->toInt() == SCRIPT_USECODE
				&& node->type == NODE_INT && node->toInt() > 255 && node->toInt() < 0x1000)
			*node = (int32_t) SetFunction(set, node->toInt());
		previous = node;
	}
}

/* Another set's engine call: its shapes and functions turned into the engine's numbers, its
 * shape results back into its own; its sounds and music are not this game's, so they are not
 * played. */
void CallFromOtherSet(EngineCall call, Value *args, Value *ret)
{
	enum GameType set = UsecodeSet();
	uint16_t i;

	if (call == (EngineCall) UC_Sfx || call == (EngineCall) UC_PlaySoundEffect2
			|| call == (EngineCall) UC_PlayMusic) {
		ret->appendInt(0);
		return;
	}
	for (i = 0; i < sizeof ShapeCalls / sizeof ShapeCalls[0]; i++) {
		if (ShapeCalls[i] == call) {
			Node *node = GetListNode(args - ShapeArgs[i], 1);
			TranslateNode(node, ShapeFromSet(set, node->toInt()));
		}
	}
	for (i = 0; i < sizeof FunctionCalls / sizeof FunctionCalls[0]; i++) {
		if (FunctionCalls[i] == call) {
			Node *node = GetListNode(args - FunctionArgs[i], 1);
			if (node->toInt() > 0 && node->toInt() < 0x1000)
				TranslateNode(node, SetFunction(set, node->toInt()));
		}
	}
	if (call == UC_Post || call == UC_PostInFuture)
		TagScriptFunctions(set, args - 2);
	(*call)(args, ret);
	if (call == UC_GetItemShape) {
		Node *node = GetListNode(ret, 1);
		TranslateNode(node, ShapeToSet(set, node->toInt()));
	}
}

uint8_t OtherFlag(int16_t flag)
{
	if (flag < 0 || flag >= OTHER_FLAG_COUNT)
		return 0;
	return (OtherFlagBits[UsecodeSet()][flag >> 3] & (0x80 >> (flag & 7))) != 0;
}

void SetOtherFlag(int16_t flag, uint8_t on)
{
	if (flag < 0 || flag >= OTHER_FLAG_COUNT)
		return;
	if (on)
		OtherFlagBits[UsecodeSet()][flag >> 3] |= 0x80 >> (flag & 7);
	else
		OtherFlagBits[UsecodeSet()][flag >> 3] &= ~(0x80 >> (flag & 7));
}

/* Written only for a set whose code has set a flag, so one-game saves keep the same files. */
void SaveOtherFlags(char *dir)
{
	for (int16_t game = 0; game < MAX_GAMES; game++) {
		uint16_t i;

		for (i = 0; i < sizeof OtherFlagBits[game] && !OtherFlagBits[game][i]; i++)
			;
		if (game == PlayedGame() || i == sizeof OtherFlagBits[game]) {
			plat_file_remove(BuildPath(dir, OtherFlagFileNames[game], 0));
			continue;
		}
		int16_t fd = CreateFileOrFail(BuildPath(dir, OtherFlagFileNames[game], 0));
		DosWrite(fd, -INT32_C(1), sizeof OtherFlagBits[game], OtherFlagBits[game]);
		DosClose(fd);
	}
}

void LoadOtherFlags(char *dir)
{
	memset(OtherFlagBits, 0, sizeof OtherFlagBits);
	for (int16_t game = 0; game < MAX_GAMES; game++) {
		if (game == PlayedGame() || !plat_file_exists(BuildPath(dir, OtherFlagFileNames[game], 0)))
			continue;
		int16_t fd = OpenFileOrFail(BuildPath(dir, OtherFlagFileNames[game], 0));
		DosRead(fd, -INT32_C(1), sizeof OtherFlagBits[game], OtherFlagBits[game]);
		DosClose(fd);
	}
}

void RemoveOtherFlagFiles(char *dir)
{
	for (int16_t game = 0; game < MAX_GAMES; game++)
		plat_file_remove(BuildPath(dir, OtherFlagFileNames[game], 0));
}

extern "C" void ResetUcsetGlobals(void)
{
	for (int16_t game = 0; game < MAX_GAMES; game++) {
		free(OtherUsecode[game]);
		free(OtherLinkdep1[game]);
		free(OtherLinkdep2[game]);
	}
	RunningSetTag = 0;
	memset(OtherUsecode, 0, sizeof OtherUsecode);
	memset(OtherLinkdep1, 0, sizeof OtherLinkdep1);
	memset(OtherLinkdep2, 0, sizeof OtherLinkdep2);
	memset(OtherLinkCount, 0, sizeof OtherLinkCount);
	memset(OtherFlagBits, 0, sizeof OtherFlagBits);
}
