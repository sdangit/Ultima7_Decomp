#ifndef BG_INTRO_CFILCACH_H
#define BG_INTRO_CFILCACH_H

#include "chkfile.h"
#include "../sound/cflxbuf.h"

namespace BgIntro {

using BgSound::SpeechCache;

/* A speech cache that reads a plain file, into a buffer from the far heap. */
struct FileSpeechCache : SpeechCache {
	DataFile file;
	FileSpeechCache();
	FileSpeechCache(int8_t streamed);
	virtual ~FileSpeechCache();
	int32_t read(void *to, uint32_t n);
	int8_t isOpen();
	uint8_t open(char *name);
	void close();
	uint32_t fileSize();
	uint8_t allocateBuffer(uint32_t n);
	void freeBuffer();
};

}

#endif
