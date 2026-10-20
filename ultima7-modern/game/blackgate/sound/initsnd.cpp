/* Black Gate U7.EXE, overlay segment 237 (file offsets 0x06d240 to 0x06d893, 1619 bytes): its
 * sound start-up and shut-down. The rest of the segment loads the world's physics data.
 * Borland C++ 2.0 -mm -O -G -P rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include "easyfile.h"
#include "flex.h"
#include "debug.h"
#include "memapi.h"
#include "init.h"
#include "plat.h"
#include "drv/adlib.h"
#include "drv/midiplay.h"
#include "sounds.h"
#include "u7sound.h"
#include "initsnd.h"

namespace BgSound {

extern void ( *DriverSysexEntry)(int32_t, int16_t, void *);
extern void ( *DriverPortEntry)(int16_t);

void InitRolandVoices(void)
{
	int16_t i, first;

	FirstVoiceChannel = SoundDriverInfo->first;
	first = SoundDriverInfo->first;
	for (i = 0; i < 32; i++) {
		VoiceSlots[i].number = first++;
		VoiceSlots[i].voice = RolandVoiceNumbers[i] - 1;
		VoiceSlots[i].sound = -1;
		VoiceSlots[i].next = &VoiceSlots[i + 1];
		VoiceSlots[i].previous = &VoiceSlots[i - 1];
	}
	VoiceSlots[0].previous = &VoiceSlots[31];
	VoiceSlots[31].next = &VoiceSlots[0];
	VoiceSlotHead = VoiceSlots;
}

void UploadRolandPatches(void)
{
	uint8_t *p;
	int16_t count;
	int32_t address;

	if (MusicDevice == MUSIC_DEVICE_MT32) {
		p = RolandPatchSetup;
		count = *p++;
		while (count != 0) {
			address = *p++;
			address <<= 8;
			address += *p++;
			address <<= 8;
			address += *p++;
			DriverSysexEntry(address, count * 4, p);
			p += count * 4;
			count = *p++;
		}
	}
}

uint8_t InitSound(void)
{
	if (MusicDevice != 0) {
		MusicFileName = MusicDevice == MUSIC_DEVICE_ADLIB ? AdlibMusicFile : Mt32MusicFile;
		StartSoundDriver(BuildPath(StaticPath, VoiceFlexName, 0), BuildPath(StaticPath, DriverFileName, 0));
		if (SoundDriverInfo == 0 || MusicDevice == 0)
			return 0;
		if (MusicDevice == MUSIC_DEVICE_ADLIB) {
			if (AdlibPort != 0x388)
				DriverPortEntry(AdlibPort);
			ResetAdlib();
			plat_sound_lock();
			SoundTickFirst = SoundTimerHandler;
			plat_sound_unlock();
			AdlibSfxActive = 1;
		}
		if (MusicDevice == MUSIC_DEVICE_MT32) {
			UploadRolandPatches();
			InitRolandVoices();
		}
		if (!AllocMusicBuffers() || !AllocSfxBuffers())
			return 0;
		ResetContinuousSounds();
	}
	return 1;
}

void ShutDownSound(void)
{
	if (MusicDevice != 0) {
		FadeOutSong(0);
		if (AdlibSfxActive != 0) {
			ReclaimAdlibChannels();
			ResetAdlib();
			plat_sound_lock();
			SoundTickFirst = 0;
			plat_sound_unlock();
		}
		StopSoundDriver();
		plat_sound_lock();
		plat_sound_tick_set(0);
		plat_sound_unlock();
		MusicDevice = 0;
	}
}

uint8_t AllocMusicBuffers(void)
{
	FlexEntry e;
	int32_t size, maxNormal, maxSpecial;
	int16_t i;
	Flex file;

	maxNormal = 0;
	maxSpecial = 0;
	MusicBuffer = 0;
	if (file.open(BuildPath(StaticPath, MusicFileName, 0))) {
		for (i = 0; i < 60; i++) {
			file.getEntry(i, &e);
			size = e.size;
			if (MusicTrackModes[i] == 1) {
				if (size > maxSpecial)
					maxSpecial = size;
			} else {
				if (size > maxNormal)
					maxNormal = size;
			}
		}
		file.close();
	}
	MusicBuffer = (uint8_t *)AllocateFarHeap(maxNormal, 0);
	/* keep the braces: they change how these returns destroy the file */
	if (MusicBuffer == 0) {
		return 0;
	}
	SpecialMusicBuffer = (uint8_t *)AllocateFarHeap(maxSpecial, 0);
	if (SpecialMusicBuffer == 0) {
		return 0;
	}
	return 1;
}

uint8_t AllocSfxBuffers(void)
{
	FlexEntry e;
	int32_t maxSize;
	void *buffer;
	int16_t i;
	Flex file;

	if (MusicDevice == MUSIC_DEVICE_MT32) {
		for (i = 0; i < SFX_COUNT; i++) {
			SfxVoices[i] = 0;
			SfxAlternate[i] = 0;
		}
	}
	if (AdlibSfxActive != 0) {
		if (file.open(BuildPath(StaticPath, AdlibSfxFile, 0))) {
			maxSize = 0;
			for (i = 0; i < SFX_COUNT; i++) {
				file.getEntry(i, &e);
				if (e.size > maxSize)
					maxSize = e.size;
			}
			file.close();
			maxSize = (maxSize + 15) & ~INT32_C(15);
			buffer = AllocateFarHeap(maxSize << 2, 2);
			if (buffer == 0)
				return 0;
			SetAdlibSoundArea((uint8_t *)buffer, maxSize << 2);
			/* each channel gets a quarter of the buffer */
			for (i = 0; i < 4; i++)
				SfxChannels[i].buffer = (uint8_t *)buffer + i * maxSize;
		} else
			return 0;
	}
	return 1;
}

}
