#ifndef UCSET_H
#define UCSET_H

#include "gametype.h"

/* Each game's usecode runs as its own set: the played game's from its files as before, another
 * game's (for its items) loaded whole at start, each with its own engine calls and flags. A
 * number another set's function is posted under carries the set above bit 11. */

struct Value;
typedef void ( *EngineCall)(Value *args, Value *ret);

/* the set running now */
enum GameType UsecodeSet(void);
uint8_t RunningOtherSet(void);
void SetUsecodeSet(enum GameType set);

/* function id of a set, as engine code and scripts carry it, and back */
uint16_t SetFunction(enum GameType set, uint16_t id);
enum GameType FunctionSet(uint16_t *func);

/* another game's usecode and its linkdep lists */
void LoadOtherUsecode(enum GameType game);
uint8_t OtherLinks(enum GameType set, uint16_t id, uint16_t *first, uint16_t *count, uint16_t *offset);
const uint8_t *OtherRecord(enum GameType set, uint16_t entry);

/* shapes as the running set numbers them: its own 0-1023, the others' from 2048 */
uint16_t ShapeToSet(enum GameType set, uint16_t shape);
int16_t ShapeFromSet(enum GameType set, int16_t value);

/* an engine call made by another set's code, with its numbers translated */
void CallFromOtherSet(EngineCall call, Value *args, Value *ret);

/* another set's flags, saved beside the game's while one is set */
uint8_t OtherFlag(int16_t flag);
void SetOtherFlag(int16_t flag, uint8_t on);
void SaveOtherFlags(char *dir);
void LoadOtherFlags(char *dir);
void RemoveOtherFlagFiles(char *dir);

#endif
