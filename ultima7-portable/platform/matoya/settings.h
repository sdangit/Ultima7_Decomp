/* Portable startup preferences shared by both games; independent of the window/audio APIs. */
#ifndef U7_SETTINGS_H
#define U7_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint32_t scale;
	char display[48];
	float scanlines;
	uint32_t music_volume;
	uint32_t speech_volume;
	char music_device;
	bool quiet_weapons;
	bool mt32_short_waits;
} U7_Settings;

void settings_defaults(U7_Settings *settings);
/* A mask records explicitly supplied fields for overlaying CLI values on loaded settings. */
bool settings_set(U7_Settings *settings, uint32_t *mask, const char *key, const char *value,
	char *error, size_t size);
void settings_overlay(U7_Settings *settings, const U7_Settings *overrides, uint32_t mask);
bool settings_load(const char *path, U7_Settings *settings, bool *missing, char *error, size_t size);
/* New files are created exclusively; replacing a validated file uses a temporary sibling. */
bool settings_write(const char *path, const U7_Settings *settings, bool replace,
	char *error, size_t size);

#endif
