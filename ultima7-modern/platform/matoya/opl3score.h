#ifndef U7_OPL3_SCORE_H
#define U7_OPL3_SCORE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum U7Opl3ScoreKind {
	U7_OPL3_SCORE_MUSIC_BANK = 1,
	U7_OPL3_SCORE_BG_INTRO_BANK,
	U7_OPL3_SCORE_XMIDI_PAIR,
	U7_OPL3_SCORE_BG_ENDING,
	U7_OPL3_SCORE_SI_MENU,
	U7_OPL3_SCORE_SI_EFFECTS
};

/* Call only for OPL3 read opens. Game is BG=0/SI=1; the caller owns a returned
 * temporary stream. NULL leaves the original file in use. */
FILE *u7_opl3_score_open(int played_game, const char *fm_path,
	const char *mt_path, int kind);

#ifdef __cplusplus
}
#endif

#endif
