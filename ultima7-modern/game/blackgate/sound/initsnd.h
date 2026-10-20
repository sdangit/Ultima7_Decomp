#ifndef BG_INITSND_H
#define BG_INITSND_H

namespace BgSound {

void InitRolandVoices(void);
void UploadRolandPatches(void);
uint8_t InitSound(void);
void ShutDownSound(void);
uint8_t AllocMusicBuffers(void);
uint8_t AllocSfxBuffers(void);

}

#endif
