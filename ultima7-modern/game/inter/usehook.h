#ifndef USEHOOK_H
#define USEHOOK_H

#include "gametype.h"

int8_t RunUsable(uint8_t event, int16_t item, uint16_t func);

uint8_t HasUsable(uint16_t func);

/* Usecode functions the engine runs by number. Each game has its own numbers; 0 means the played
 * game has no such function. */
enum UsecodeHook {
	HOOK_SPEECH_TEXT,       /* a speech shown as text */
	HOOK_AVATAR_GONE,       /* the avatar is taken out of play */
	HOOK_BED,
	HOOK_ARREST,
	HOOK_BARGE,             /* the party sat down on a barge */
	HOOK_BREAK_ITEM,
	HOOK_THEFT,             /* a theft seen by someone hostile */
	HOOK_SCOLD,             /* a theft seen by the party, or the party attacking its own */
	HOOK_FIRST_SPELL,       /* spell n runs this plus n */
	HOOK_BOOK,              /* books of quality 100 and up */
	HOOK_LATE_BOOK,         /* books of quality 151 and up */
	HOOK_SERPENT_BOOK,      /* shape 705 of quality 211 and up */
	HOOK_OINK,              /* every NPC in oink mode */
	HOOK_HOOK_DIES,         /* Hook (shape 506) is killed */
	HOOK_FEED,              /* the F key */
	HOOK_KEY_REFUSED,       /* the key ring turns a key away */
	HOOK_MOONSHADE,         /* Moonshade ravaged on restore */
	HOOK_MAZE_PATROL,       /* a patroller with id 31 */
	HOOK_MONITOR_HIT,       /* hit by shape 591 */
	HOOK_SLEEP_ARROW_HIT,   /* hit by shape 568 */
	HOOK_COUNT
};

uint16_t UsecodeHook(enum UsecodeHook hook);
uint16_t GameUsecodeHook(enum GameType game, enum UsecodeHook hook);
void PostUsecodeHook(uint16_t item, enum UsecodeHook hook);
uint8_t TalkEvent(void);

extern int16_t TelekinesisUsable;
extern int8_t UsableRunning;

#endif
