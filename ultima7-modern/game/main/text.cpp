/* Serpent Isle SI.EXE, overlay segment 244 (file offsets 0x069ea0 to 0x06a031, 401 bytes).
 * Borland C++ 2.0 -mm -O -G -P -d -Y rebuilds it byte for byte as C++.
 */

#include "u7port.h"
#include <new>
#include <string.h>
#include "easyfile.h"
#include "flex.h"
#include "dcache.h"
#include "debug.h"
#include "u7ibuf.h"
#include "oops.h"
#include "text.h"
#include "rules.h"
#include "plat.h"
#include "gametype.h"

/* The game's text, read from text.flx. */
struct TextCache : DataCache {
	char *text(uint16_t section, uint16_t n);
	char *read(int16_t n, int16_t *len, char *dst);
};

static char *const TextFileName = "text.flx";
static TextCache GameTextCache;
/* The game whose item names sections 0 and 2 give, or -1 for the played game's. */
static int8_t NameGame = -1;
/* Shown for an entry out of range. */
static char *const MissingText = "NA";
/* The first entry of each section. The last is the word SI.EXE stored after the table, the "te"
 * of "text.flx". */
static const uint16_t TextSectionStart[9] = { 0, 1024, 1280, 1670, 1926, 2182, 2438, 2538, 0x6574 };
/* Black Gate's text has seven sections; the eighth is empty. */
static const uint16_t BgTextSectionStart[9] = { 0, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 2560 };

/* Section 3 text the engine uses that Black Gate's file lacks. */
static const uint16_t BgExtraTextNumbers[] = {
	171, 172, 173, 174, 212, 239, 240, 242, 243, 244, 245, 247, 248,
};
static const char *const BgExtraText[] = {
	"Right-handed mouse", "Left-handed mouse", "Frame Limiting on", "Frame Limiting off", "Locked",
	"%2d", "%s %d", "n/a", "%6ld", "%02d/%02d", "%3d", "Audio on", "Audio off",
};

static char *FindBgExtraText(uint16_t n)
{
	for (uint16_t i = 0; i < sizeof BgExtraTextNumbers / sizeof BgExtraTextNumbers[0]; i++)
		if (BgExtraTextNumbers[i] == n)
			return (char *)BgExtraText[i];
	return 0;
}

/* The cheat tools' text, which Black Gate's file lacks (sections 5 and 7, section 3 past entry
 * 52), comes from Serpent Isle's. */
static uint8_t IsSiToolText(uint16_t section, uint16_t n)
{
	return section == 5 || section == 7 || (section == 3 && n > 52);
}

static char *ReadGameText(GameType game, uint16_t record)
{
	char line[256];
	uint8_t entry[8];
	int32_t offset, length;
	int16_t file = plat_game_file_open(game, "STATIC\\TEXT.FLX");

	if (file < 0)
		return MissingText;
	length = 0;
	if (plat_file_seek(file, 0x80 + 8 * (int32_t) record, PLAT_SEEK_SET) >= 0
		&& plat_file_read(file, entry, 8) == 8) {
		offset = entry[0] | entry[1] << 8 | (int32_t) entry[2] << 16 | (int32_t) entry[3] << 24;
		length = entry[4] | entry[5] << 8;
		if (length > (int32_t) sizeof line - 1)
			length = sizeof line - 1;
		if (plat_file_seek(file, offset, PLAT_SEEK_SET) != offset
			|| plat_file_read(file, line, length) != length)
			length = 0;
	}
	plat_file_close(file);
	line[length] = '\0';
	return StorePath(line);
}

char *TextCache::text(uint16_t section, uint16_t n)
{
	char *s;
	const uint16_t *start = PlayingBlackGate() ? BgTextSectionStart : TextSectionStart;

	if (PlayingBlackGate() && section == 3 && FindBgExtraText(n))
		s = StorePath(FindBgExtraText(n));
	else if (PlayingBlackGate() && IsSiToolText(section, n))
		s = ReadGameText(GAME_SI, TextSectionStart[section] + n);
	else if (section == 0 && IsForeignShape(n))
		s = ReadGameText(ShapeGame(n), n & 0x3ff);
	else if (NameGame >= 0 && NameGame != PlayedGame() && (section == 0 || section == 2)) {
		/* another game's item names, from its own file and sections */
		const uint16_t *own = NameGame == GAME_BG ? BgTextSectionStart : TextSectionStart;

		s = n < own[section + 1] - own[section] ? ReadGameText((GameType)NameGame, own[section] + n) : MissingText;
	}
	else if (section >= 0 && section <= 7) {
		/* Section 7 reads its end from past the table. */
		if (n < 0 || n >= (uint16_t)(start[section + 1] - start[section]))
			s = MissingText;
		else
			s = StorePath(get(start[section] + n));
	} else
		s = MissingText;
	return s;
}

char *TextCache::read(int16_t n, int16_t *len, char *dst)
{
	Flex f;

	if (!f.open(BuildPath(StaticPath, TextFileName, 0)))
		ReportFileNotFound(BuildPath(StaticPath, TextFileName, 0));
	if (!f.readRecord(n, dst, 0))
		strcpy(dst, "");
	f.close();
	*len = strlen(dst) + 1;
	return dst;
}

char *GetGameText(uint8_t section, uint16_t n)
{
	if (OinkMode)
		return GameTextCache.text(3, 220);
	return GameTextCache.text(section, n);
}

void SetItemNameGame(int8_t game)
{
	NameGame = game;
}

void InitTextCache(void)
{
	GameTextCache.init(1024);
}

extern "C" void ResetTextGlobals(void)
{
	memset((void *)&GameTextCache, 0, sizeof(GameTextCache));
	NameGame = -1;
}

extern "C" void ConstructTextGlobals(void)
{
	new (&GameTextCache) TextCache();
}
