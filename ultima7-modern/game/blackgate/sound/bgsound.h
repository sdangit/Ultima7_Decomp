#ifndef BG_BGSOUND_H
#define BG_BGSOUND_H

/* Black Gate's in-game sound: the engine's sound calls hand over to these while Black Gate plays. */

#include "initsnd.h"
#include "sounds.h"
#include "voice.h"

namespace BgSound {

void EnableMusic(int16_t on);
void PlayMusic(uint8_t track);
void StopMusic();
void PlayCombatMusic();
void PlayPainSfx();
void PlaySfx(int16_t number, int16_t volume, int16_t pan, int16_t flags);
void StopSfx(uint8_t number);
void SetSfxVolume(uint8_t number, int16_t volume);
void EnableSfx(int8_t enabled);

}

#endif
