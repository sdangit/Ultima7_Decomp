#ifndef GAMETYPE_H
#define GAMETYPE_H

#include "plat.h"

/* Which game's files something comes from. The played game is one of the first two; mods are
 * extra sets of files played alongside it. */
enum GameType {
	GAME_BG = 0,
	GAME_SI = 1,
	GAME_MOD1 = 2,
	GAME_MOD2 = 3,
	MAX_GAMES = 4
};

static inline enum GameType PlayedGame(void)
{
	return (enum GameType) plat_played_game();
}

/* Shape numbers in memory: the played game's keep 0-1023; another game's shape n is
 * FIRST_FOREIGN_SHAPE + game * 1024 + n, past every art handle, so it is its own handle. */
#define FIRST_FOREIGN_SHAPE 2048
#define SHAPE_NUMBERS (FIRST_FOREIGN_SHAPE + MAX_GAMES * 1024)

static inline uint8_t IsForeignShape(uint16_t shape) { return shape >= FIRST_FOREIGN_SHAPE; }

static inline enum GameType ShapeGame(uint16_t shape)
{
	return IsForeignShape(shape) ? (enum GameType) ((shape - FIRST_FOREIGN_SHAPE) >> 10) : PlayedGame();
}

/* Shape n of a game, as numbered in memory. */
static inline uint16_t GameShape(enum GameType game, uint16_t n)
{
	return game == PlayedGame() ? n : (uint16_t) (FIRST_FOREIGN_SHAPE + game * 1024 + n);
}

#endif
