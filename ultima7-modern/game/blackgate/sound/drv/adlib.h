#ifndef BG_ADLIB_H
#define BG_ADLIB_H

namespace BgSound {
void ReclaimAdlibChannels(void);
int16_t GetAdlibVoiceState(int16_t);
void StopAdlibVoice(int16_t);
void TickAdlibSounds();
void YieldAdlibChannels();
int16_t StartAdlibSound(uint8_t *, int16_t, int16_t pan = 64);
void SetAdlibVolume(int16_t, int16_t);
void SetAdlibPan(int16_t, int16_t);
void ResetAdlib(void);
void SetAdlibSoundArea(uint8_t *start, int32_t size);
}

#endif
