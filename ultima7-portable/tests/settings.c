/* No game data, display, audio device, or third-party libraries are needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "settings.h"

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static void put(const char *path, const char *text, size_t size)
{
	FILE *file = fopen(path, "wb");
	CHECK(file != NULL);
	CHECK(fwrite(text, 1, size, file) == size);
	CHECK(fclose(file) == 0);
}

static void get(const char *path, char *text, size_t size)
{
	FILE *file = fopen(path, "rb");
	size_t n;
	CHECK(file != NULL);
	n = fread(text, 1, size - 1, file);
	CHECK(!ferror(file));
	text[n] = '\0';
	CHECK(fclose(file) == 0);
}

int main(int argc, char **argv)
{
	char directory[4096], path[4096], error[4608], contents[8192], original[8192];
	U7_Settings settings, overrides, loaded;
	uint32_t mask = 0;
	bool missing;
	static const struct { const char *key, *value; } invalid[] = {
		{"scale", "0"}, {"scale", "-1"}, {"scale", "13421773"}, {"scale", "1.5"},
		{"scale", "99999999999999999999999"}, {"scale", ""}, {"scale", " 3"},
		{"display", "0"}, {"display", "-2"}, {"display", "1.5"}, {"display", "other"},
		{"display", "uuid:00000000-0000-0000-0000-00000000000z"}, {"display", "uuid:"},
		{"scanlines", "nan"}, {"scanlines", "inf"}, {"scanlines", "-0.1"},
		{"scanlines", "1.01"}, {"scanlines", "0.3x"}, {"scanlines", ""},
		{"music_volume", "101"}, {"music_volume", "60.5"}, {"music_volume", "-1"},
		{"speech_volume", "200"}, {"speech_volume", "fifty"},
		{"music_device", "midi"}, {"music_device", "AUTO"},
		{"quiet_weapons", "yes"}, {"quiet_weapons", "1"}, {"mt32_short_waits", "FALSE"},
		{"unknown", "1"}
	};
	static const struct { const char *text, *detail; unsigned line; } bad_files[] = {
		{"# comment\nmusic_volume = 150\n", "music_volume=150", 2},
		{"scale = 3\nscale = 4\n", "duplicate", 2},
		{"scale 3\n", "expected setting = value", 1},
		{"musci_volume = 50\n", "unknown setting", 1},
		{"speech_volume =\n", "speech_volume=", 1},
		{"music_device = roland\nscanlines = nan\n", "scanlines=nan", 2}
	};

	CHECK(argc == 2);
	CHECK(snprintf(directory, sizeof directory, "%s/settings-test-%lu", argv[1], (unsigned long) getpid()) < (int) sizeof directory);
	CHECK(mkdir(directory, 0700) == 0);
	CHECK(snprintf(path, sizeof path, "%s/settings.cfg", directory) < (int) sizeof path);
	settings_defaults(&settings);
	CHECK(settings.scale == 3 && strcmp(settings.display, "primary") == 0);
	CHECK(settings.scanlines == 0 && settings.music_volume == 100 && settings.speech_volume == 100);
	CHECK(settings.music_device == 0 && !settings.quiet_weapons && !settings.mt32_short_waits);
	CHECK(settings_load(path, &settings, &missing, error, sizeof error) && missing);
	CHECK(settings_write(path, &settings, false, error, sizeof error));
	get(path, original, sizeof original);
	CHECK(strstr(original, "#") && strstr(original, "music_volume = 100"));
	CHECK(!settings_write(path, &settings, false, error, sizeof error));
	get(path, contents, sizeof contents);
	CHECK(strcmp(original, contents) == 0);

	for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
		uint32_t before = mask;
		CHECK(!settings_set(&settings, &mask, invalid[i].key, invalid[i].value, error, sizeof error));
		CHECK(mask == before && strstr(error, invalid[i].key));
	}
	CHECK(!settings_set(&settings, &mask, "scale", NULL, error, sizeof error));

	const char *valid = "\xef\xbb\xbf# comments and BOM\r\n scale = 4 ; inline\r\n"
		"scanlines = .15 # darkness\n music_volume = 60\nspeech_volume = 80\n"
		"music_device = adlib\nquiet_weapons = true\nmt32_short_waits = true\n"
		"display = uuid:AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
	put(path, valid, strlen(valid));
	settings_defaults(&settings);
	CHECK(settings_load(path, &settings, &missing, error, sizeof error) && !missing);
	CHECK(settings.scale == 4 && settings.scanlines == 0.15f);
	CHECK(settings.music_volume == 60 && settings.speech_volume == 80 && settings.music_device == 'a');
	CHECK(settings.quiet_weapons && settings.mt32_short_waits);
	CHECK(strcmp(settings.display, "uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") == 0);

	settings_defaults(&overrides);
	mask = 0;
	CHECK(settings_set(&overrides, &mask, "speech_volume", "0", error, sizeof error));
	CHECK(settings_set(&overrides, &mask, "quiet_weapons", "false", error, sizeof error));
	settings_overlay(&settings, &overrides, mask);
	CHECK(settings.speech_volume == 0 && !settings.quiet_weapons);
	CHECK(settings.scale == 4 && settings.music_volume == 60 && settings.mt32_short_waits);
	CHECK(settings_write(path, &settings, true, error, sizeof error));
	settings_defaults(&loaded);
	CHECK(settings_load(path, &loaded, &missing, error, sizeof error));
	CHECK(loaded.scale == settings.scale && loaded.scanlines == settings.scanlines);
	CHECK(loaded.music_volume == 60 && loaded.speech_volume == 0 && loaded.music_device == 'a');
	CHECK(!loaded.quiet_weapons && loaded.mt32_short_waits && strcmp(loaded.display, settings.display) == 0);

	/* Missing keys use defaults; valid zero values must remain explicit overrides. */
	put(path, "music_volume = 0", 16);
	settings_defaults(&loaded);
	CHECK(settings_load(path, &loaded, &missing, error, sizeof error));
	CHECK(loaded.music_volume == 0 && loaded.speech_volume == 100 && loaded.scale == 3);
	CHECK(settings_set(&loaded, NULL, "display", "0000000000000000000000000000000000000000000000002", error, sizeof error));
	CHECK(strcmp(loaded.display, "2") == 0);
	CHECK(settings_set(&loaded, NULL, "music_device", "roland", error, sizeof error) && loaded.music_device == 'r');
	CHECK(settings_set(&loaded, NULL, "music_device", "auto", error, sizeof error) && loaded.music_device == 0);

	for (size_t i = 0; i < sizeof bad_files / sizeof *bad_files; i++) {
		char line[32];
		put(path, bad_files[i].text, strlen(bad_files[i].text));
		settings_defaults(&loaded);
		CHECK(!settings_load(path, &loaded, &missing, error, sizeof error));
		snprintf(line, sizeof line, ":%u:", bad_files[i].line);
		CHECK(strstr(error, path) && strstr(error, line) && strstr(error, bad_files[i].detail));
		get(path, contents, sizeof contents);
		CHECK(strcmp(contents, bad_files[i].text) == 0);
	}
	put(path, "scale=3\0ignored", 15);
	CHECK(!settings_load(path, &loaded, &missing, error, sizeof error) && strstr(error, "NUL"));
	memset(contents, '#', 2048);
	put(path, contents, 2048);
	CHECK(!settings_load(path, &loaded, &missing, error, sizeof error) && strstr(error, "too long"));
	put(path, "", 0);
	settings_defaults(&loaded);
	CHECK(settings_load(path, &loaded, &missing, error, sizeof error) && !missing && loaded.scale == 3);
	CHECK(remove(path) == 0);
	CHECK(rmdir(directory) == 0);
	puts("Settings defaults, validation, comments, precedence, persistence and file preservation passed.");
	return 0;
}
