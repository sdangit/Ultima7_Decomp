#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "settings.h"

typedef enum { SETTING_INTEGER, SETTING_FRACTION, SETTING_BOOLEAN, SETTING_DEVICE, SETTING_DISPLAY } setting_type;
typedef struct {
	const char *key;
	setting_type type;
	size_t offset, bytes;
	uint32_t minimum, maximum;
	const char *values;
	const char *comment;
} setting;

#define FIELD(name) offsetof(U7_Settings, name), sizeof(((U7_Settings *) 0)->name)
static const setting fields[] = {
	{"scale", SETTING_INTEGER, FIELD(scale), 1, UINT32_MAX / 320,
		"a whole number from 1 to 13421772", "Initial window size: whole number 1..13421772 times 320x240. Default 3. Reduced to fit; macOS uses points."},
	{"display", SETTING_DISPLAY, FIELD(display), 0, 0,
		"primary, a positive display number, or uuid: followed by a display UUID",
		"macOS only: primary, a number from --list-displays, or uuid:UUID. Saving a number stores its UUID.\n# Other platforms retain this preference but ignore it. A disconnected saved display uses primary."},
	{"scanlines", SETTING_FRACTION, FIELD(scanlines), 0, 1,
		"a finite number from 0 to 1", "Scanline darkness: 0=off, 0.3=subtle, 1=black bands. Default 0."},
	{"music_volume", SETTING_INTEGER, FIELD(music_volume), 0, 100,
		"a whole-number percentage from 0 to 100", "Combined music/SFX output volume: 0=mute, 100=original level. Default 100; no boost."},
	{"speech_volume", SETTING_INTEGER, FIELD(speech_volume), 0, 100,
		"a whole-number percentage from 0 to 100", "Speech output volume: 0=mute, 100=original level. Default 100; no boost."},
	{"music_device", SETTING_DEVICE, FIELD(music_device), 0, 0,
		"auto, adlib, or roland", "Music device: auto=MT-32 when its ROMs are found, else AdLib; adlib or roland. Default auto."},
	{"quiet_weapons", SETTING_BOOLEAN, FIELD(quiet_weapons), 0, 0,
		"true or false", "true/false: suppress continuous magical-weapon sounds (fire sword/firedoom staff in Serpent Isle). Default false."},
	{"mt32_short_waits", SETTING_BOOLEAN, FIELD(mt32_short_waits), 0, 0,
		"true or false", "true/false: skip original MT-32 memory-write settle waits. Default false."},
};
#undef FIELD
#define FIELD_COUNT (sizeof fields / sizeof *fields)

void settings_defaults(U7_Settings *settings)
{
	*settings = (U7_Settings) {.scale = 3, .display = "primary", .music_volume = 100,
		.speech_volume = 100};
}

static bool integer(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value)
{
	char *end;
	unsigned long n;

	if (text == NULL || text[0] < '0' || text[0] > '9')
		return false;
	errno = 0;
	n = strtoul(text, &end, 10);
	if (errno != 0 || *end != '\0' || n < minimum || n > maximum)
		return false;
	*value = (uint32_t) n;
	return true;
}

static bool display_value(const char *text, char *out)
{
	uint32_t number;

	if (integer(text, 1, UINT32_MAX, &number)) {
		snprintf(out, sizeof ((U7_Settings *) 0)->display, "%u", number);
		return true;
	}
	if (strcmp(text, "primary") != 0) {
		if (strlen(text) != 41 || strncmp(text, "uuid:", 5) != 0)
			return false;
		for (size_t i = 5; i < 41; i++) {
			bool dash = i == 13 || i == 18 || i == 23 || i == 28;
			if (dash ? text[i] != '-' : !isxdigit((unsigned char) text[i]))
				return false;
		}
	}
	for (size_t i = 0; (out[i] = (char) tolower((unsigned char) text[i])) != '\0'; i++) {}
	return true;
}

bool settings_set(U7_Settings *settings, uint32_t *mask, const char *key, const char *value,
	char *error, size_t size)
{
	for (size_t i = 0; i < FIELD_COUNT; i++) {
		const setting *field = &fields[i];
		void *out = (char *) settings + field->offset;
		bool valid = false;

		if (strcmp(key, field->key) != 0)
			continue;
		if (value != NULL) {
			switch (field->type) {
			case SETTING_INTEGER:
				valid = integer(value, field->minimum, field->maximum, out);
				break;
			case SETTING_FRACTION: {
				char *end;
				float n;

				errno = 0;
				n = strtof(value, &end);
				valid = (value[0] == '.' || (value[0] >= '0' && value[0] <= '9'))
					&& errno == 0 && end != value && *end == '\0' && isfinite(n)
					&& n >= field->minimum && n <= field->maximum;
				if (valid) *(float *) out = n;
				break;
			}
			case SETTING_BOOLEAN:
				valid = strcmp(value, "true") == 0 || strcmp(value, "false") == 0;
				if (valid) *(bool *) out = strcmp(value, "true") == 0;
				break;
			case SETTING_DEVICE:
				valid = strcmp(value, "auto") == 0 || strcmp(value, "adlib") == 0
					|| strcmp(value, "roland") == 0;
				if (valid) *(char *) out = strcmp(value, "auto") == 0 ? 0 : value[0];
				break;
			case SETTING_DISPLAY:
				valid = display_value(value, out);
				break;
			}
		}
		if (!valid) {
			snprintf(error, size, "%s=%s is invalid; expected %s", key,
				value != NULL ? value : "<missing>", field->values);
			return false;
		}
		if (mask != NULL) *mask |= UINT32_C(1) << i;
		return true;
	}
	snprintf(error, size, "unknown setting '%s'", key);
	return false;
}

void settings_overlay(U7_Settings *settings, const U7_Settings *overrides, uint32_t mask)
{
	for (size_t i = 0; i < FIELD_COUNT; i++)
		if (mask & (UINT32_C(1) << i))
			memcpy((char *) settings + fields[i].offset,
				(const char *) overrides + fields[i].offset, fields[i].bytes);
}

static char *trim(char *text)
{
	char *end;

	while (isspace((unsigned char) *text)) text++;
	end = text + strlen(text);
	while (end > text && isspace((unsigned char) end[-1])) *--end = '\0';
	return text;
}

bool settings_load(const char *path, U7_Settings *settings, bool *missing, char *error, size_t size)
{
	FILE *file = fopen(path, "rb");
	char line[1024], detail[256] = "";
	unsigned number = 0;
	uint32_t seen = 0;
	bool ok = true;

	*missing = false;
	if (file == NULL) {
		if (errno == ENOENT) { *missing = true; return true; }
		snprintf(error, size, "%s: could not read settings: %s", path, strerror(errno));
		return false;
	}
	for (;;) {
		char *text = line, *equal;
		uint32_t before = seen;
		size_t length = 0;
		int c;

		number++;
		while ((c = fgetc(file)) != EOF && c != '\n') {
			if (c == 0 || length >= sizeof line - 1) {
				snprintf(detail, sizeof detail, c == 0 ? "NUL byte in settings" : "line is too long (maximum 1023 bytes)");
				ok = false; break;
			}
			line[length++] = (char) c;
		}
		if (!ok) break;
		if (c == EOF && length == 0) { number--; break; }
		line[length] = '\0';
		if (number == 1 && strncmp(text, "\xef\xbb\xbf", 3) == 0) text += 3;
		text[strcspn(text, "#;")] = '\0';
		text = trim(text);
		if (*text == '\0') continue;
		equal = strchr(text, '=');
		if (equal == NULL) {
			snprintf(detail, sizeof detail, "expected setting = value");
			ok = false; break;
		}
		*equal++ = '\0';
		text = trim(text);
		if (!settings_set(settings, &seen, text, trim(equal), detail, sizeof detail)) {
			ok = false; break;
		}
		if (seen == before) {
			snprintf(detail, sizeof detail, "duplicate setting '%s'", text);
			ok = false; break;
		}
	}
	if (ok && ferror(file)) {
		snprintf(detail, sizeof detail, "could not read settings: %s", strerror(errno));
		ok = false;
	}
	fclose(file);
	if (!ok) snprintf(error, size, "%s:%u: %s", path, number, detail);
	return ok;
}

static void write_values(FILE *file, const U7_Settings *settings)
{
	fprintf(file, "# Ultima VII portable settings. Black Gate and Serpent Isle each use their own file.\n"
		"# Lines use key = value. # and ; introduce comments. Names and keywords are case-sensitive.\n"
		"# Omitted keys use internal defaults. Invalid/unknown/duplicate settings stop launch.\n"
		"# CLI options override this file for one launch. --save-settings writes effective settings.\n\n");
	for (size_t i = 0; i < FIELD_COUNT; i++) {
		const setting *field = &fields[i];
		const void *value = (const char *) settings + field->offset;

		fprintf(file, "# %s\n%s = ", field->comment, field->key);
		switch (field->type) {
		case SETTING_INTEGER: fprintf(file, "%u", *(const uint32_t *) value); break;
		case SETTING_BOOLEAN: fputs(*(const bool *) value ? "true" : "false", file); break;
		case SETTING_DEVICE: fputs(*(const char *) value == 'a' ? "adlib" : *(const char *) value == 'r' ? "roland" : "auto", file); break;
		case SETTING_DISPLAY: fputs(value, file); break;
		case SETTING_FRACTION: {
			char text[32];
			float n = *(const float *) value;
			/* Shortest significant-digit representation that round-trips this float. */
			for (int precision = 1; precision <= 9; precision++) {
				snprintf(text, sizeof text, "%.*g", precision, n);
				if (strtof(text, NULL) == n) break;
			}
			fputs(text, file);
			break;
		}
		}
		fputs("\n\n", file);
	}
}

bool settings_write(const char *path, const U7_Settings *settings, bool replace,
	char *error, size_t size)
{
	char temporary[4096];
	const char *output = path;
	FILE *file = NULL;
	bool ok;

	if (replace) {
		for (unsigned i = 0; i < 100; i++) {
			if (snprintf(temporary, sizeof temporary, "%s.tmp.%lu.%u", path,
				(unsigned long) getpid(), i) >= (int) sizeof temporary) {
				snprintf(error, size, "%s: settings path is too long", path);
				return false;
			}
			file = fopen(temporary, "wx");
			if (file != NULL || errno != EEXIST) break;
		}
		output = temporary;
	} else {
		file = fopen(path, "wx");
	}
	if (file == NULL) {
		snprintf(error, size, "%s: could not write settings: %s", path, strerror(errno));
		return false;
	}
	write_values(file, settings);
	ok = !ferror(file);
	if (fclose(file) != 0) ok = false;
	if (!ok) {
		snprintf(error, size, "%s: could not finish writing settings: %s", path, strerror(errno));
		remove(output);
		return false;
	}
	if (replace) {
#ifdef _WIN32
		ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
		if (!ok) snprintf(error, size, "%s: could not replace settings (Windows error %lu)", path, GetLastError());
#else
		ok = rename(temporary, path) == 0;
		if (!ok) snprintf(error, size, "%s: could not replace settings: %s", path, strerror(errno));
#endif
		if (!ok) remove(temporary);
	}
	return ok;
}
