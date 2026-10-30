/* Black Gate shared module MUSIC, linked into MAINMENU.EXE and INTRO.EXE: the music driver,
 * songs and MIDI sound effects, played through U7's MIDI player.
 */

#include "u7port.h"
#include "plat.h"
#include "oops.h"
#include "memapi.h"
#include "flex.h"
#include "errors.h"
#include "music.h"

namespace BgSound {
extern void (*DriverSysexEntry)(int32_t, int16_t, void *);
}

namespace BgShared {

#define MUSIC_DEVICE_THIRD  3       /* a driver U7 no longer offers */

char MusicEnabled = 0;
char SfxEnabled = 0;

/* The game's MT-32 patch setup: runs of a count, a 3-byte address and count * 4 bytes. */
static const uint8_t RolandPatchSetup[77] = {
	1, 3, 1, 32, 0, 90, 7, 0, 1, 3, 1, 52, 6, 100, 7, 1,
	1, 3, 2, 88, 1, 90, 5, 0, 12, 3, 2, 96, 1, 90, 6, 0,
	1, 90, 7, 0, 2, 100, 7, 1, 1, 90, 8, 0, 5, 90, 7, 1,
	1, 90, 9, 0, 3, 95, 7, 1, 4, 100, 4, 1, 4, 100, 5, 1,
	4, 100, 6, 1, 4, 100, 7, 1, 4, 100, 8, 1, 0
};

static void UploadRolandPatches(void)
{
	const uint8_t *p;
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
			DriverSysexEntry(address, count * 4, (void *) p);
			p += count * 4;
			count = *p++;
		}
	}
}

void MusicSystem::start(uint8_t device, char *driverFile, char *timbreFile)
{
	if (!plat_file_exists(timbreFile))
		ReportFileNotFound(timbreFile);
	if (!plat_file_exists(driverFile))
		ReportFileNotFound(driverFile);
	if (device) {
		switch (device) {
		case MUSIC_DEVICE_MT32:
			MusicDevice = MUSIC_DEVICE_MT32;
			break;
		case MUSIC_DEVICE_ADLIB:
			MusicDevice = MUSIC_DEVICE_ADLIB;
			break;
		case MUSIC_DEVICE_THIRD:
			MusicDevice = MUSIC_DEVICE_THIRD;
			break;
		}
		StartSoundDriver(timbreFile, driverFile);
		if (MusicDevice == 0)
			FatalError("Unable to initialize sound driver.\n"
				"Refer to your reference for concerning\n"
				"sound board configuration.\n");
		/* the same patch table the game uploads */
		if (MusicDevice == MUSIC_DEVICE_MT32)
			UploadRolandPatches();
	} else
		MusicDevice = 0;
}

void MusicSystem::stop()
{
	if (MusicReady()) {
		FadeOutSong(0);
		StopSoundDriver();
		plat_sound_lock();
		plat_sound_tick_set(0);
		plat_sound_unlock();
		MusicDevice = 0;
	}
}

Song::Song()
{
	data = 0;
}

Song::Song(MusicSystem *, char *name)
{
	data = 0;
	if (MusicEnabled == 0 || !MusicReady())
		return;
	load(name);
}

Song::Song(MusicSystem *, char *flexName, int16_t entry)
{
	data = 0;
	if (MusicEnabled == 0 || !MusicReady())
		return;
	load(flexName, entry);
}

Song::~Song()
{
	if (MusicEnabled)
		FadeOutSong(0);
	if (data != 0)
		FreeFarHeap(data);
}

void Song::load(char *name)
{
	int32_t size;

	if (MusicEnabled == 0)
		return;
	if (data != 0)
		FreeFarHeap(data);
	data = 0;
	if (!file.open(name, FILE_OPEN))
		ReportFileNotFound(name);
	size = file.getLength();
	data = (uint8_t *) AllocateFarHeap(size, 0);
	if (data == 0)
		ReportOutOfFarMemory();
	file.read(data, size);
}

void Song::load(char *flexName, int16_t entry)
{
	int32_t size;
	FlexEntry where;

	if (MusicEnabled == 0)
		return;
	if (data != 0)
		FreeFarHeap(data);
	data = 0;
	Flex flex;
	if (!flex.open(flexName))
		ReportFileNotFound(flexName);
	flex.getEntry(entry, &where);
	size = where.size;
	data = (uint8_t *) AllocateFarHeap(size, 0);
	if (data == 0)
		ReportOutOfFarMemory();
	flex.readEntry(&where, data, 0);
	flex.close();
}

void Song::play()
{
	if (data != 0 && MusicEnabled)
		PlaySong(data);
}

void Song::fadeOut(uint16_t ticks)
{
	if (MusicEnabled)
		FadeOutSong(ticks);
}

uint8_t Song::finished()
{
	if (MusicEnabled)
		return MusicFlags & MUSIC_ENDED;
	return 1;
}

void Song::unload()
{
	if (data != 0) {
		FreeFarHeap(data);
		data = 0;
	}
}

void EnableMusic()
{
	MusicEnabled = 1;
}

void DisableMusic()
{
	MusicEnabled = 0;
}

SoundEffect::SoundEffect(SfxNote &n) : note(n)
{
	setup(0, SFX_FULL_VOLUME, SFX_PAN_CENTER, 0, 1);
}

SoundEffect::SoundEffect(uint8_t how, uint8_t program, uint8_t key, uint8_t loudness,
	int16_t ticks, uint8_t slide, uint8_t skip) : note(how, program, key, loudness, ticks, slide, skip)
{
	setup(0, SFX_FULL_VOLUME, SFX_PAN_CENTER, 0, 1);
}

void SoundEffect::setup(int16_t f, int16_t v, int16_t p, int16_t n, int16_t level)
{
	flags = f;
	volume = v;
	pan = p;
	id = n;
	priority = level;
}

void SoundEffect::play(int16_t n)
{
	if (n > 0)
		id = n;
	if (SfxEnabled)
		StartMidiSfx((uint8_t *) &note, flags, volume, pan, id, priority);
}

void SoundEffect::stop()
{
	if (SfxEnabled)
		StopMidiSfx(0, id);
}

void EnableSfx()
{
	SfxEnabled = 1;
}

void DisableSfx()
{
	SfxEnabled = 0;
}

}

extern "C" void ResetBgSharedMusicGlobals(void)
{
	BgShared::MusicEnabled = 0;
	BgShared::SfxEnabled = 0;
}
