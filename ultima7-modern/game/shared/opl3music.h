#ifndef OPL3MUSIC_H
#define OPL3MUSIC_H

#include <stdint.h>

namespace NativeOpl3 {

constexpr int MusicVoices = 18;
/* BG reserves four channels for effects, keeping their note-offs separate. */
constexpr int BgMusicVoices = 14;
constexpr int BgEffectVoices = 4;

inline uint16_t ChannelRegister(int voice)
{
	return (voice >= 9 ? 0x100 : 0) + voice % 9;
}

inline uint16_t ModulatorRegister(int voice)
{
	static const uint8_t slots[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };
	return (voice >= 9 ? 0x100 : 0) + slots[voice % 9];
}

int16_t Init(void *timbres, int voices);
void SetPort(int16_t port);
void Shutdown(void);
void Note(int16_t channel, int16_t note, int16_t velocity);
void Pitch(int16_t channel, int16_t low, int16_t high);
void Control(int16_t channel, int16_t control, int16_t value);
void Program(int16_t channel, int16_t program);
void Timer(void);

}

#endif
