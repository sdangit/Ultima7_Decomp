/* Gradual lighting: the time of day picks its palette from the exact minute, and changes of
 * light (a cave, a torch, the light spell, weather, invisibility) cross-fade over a moment.
 *
 *   darkness   0 ............ 0.5 ............ 1
 *   no light   day/weather -> dusk     ->     night
 *   a light    day/weather ------------->     the light's palette
 *
 * A cave shows the dark end whatever the hour. The original rules still decide state, light
 * spell time and lightning; only the colors they set come from here.
 */

#include "u7port.h"
#include "plat.h"
#include "arena.h"
#include "crawpal.h"
#include "gtimer.h"
#include "weather.h"
#include "collide.h"
#include "rules.h"
#include "worldpal.h"
#include "palfade.h"

#define COMPONENTS 768

/* minutes after midnight */
#define DUSK_START (18 * 60)
#define DAWN_START (4 * 60)
#define TWILIGHT_MINUTES 180

/* a light, a cave or invisibility; weather changes more slowly */
#define LIGHT_FADE_MS 500
#define WEATHER_FADE_MS 1500
/* a longer gap, as while the clock is held for a conversation, counts as this much */
#define LONGEST_STEP_MS 250

/* the palettes a scene blends; a change of any starts a cross-fade */
struct Scene {
	int32_t day, dark;
	uint8_t dungeon, invisible;
	uint8_t operator!=(const Scene &o) const
	{
		return day != o.day || dark != o.dark || dungeon != o.dungeon || invisible != o.invisible;
	}
};

uint8_t PaletteFrameUpdate;

static float Shown[COMPONENTS], FadeFrom[COMPONENTS];
static uint32_t FadeMs, FadeLength = LIGHT_FADE_MS, LastMs, LastTick;
static Scene LastScene;
static uint8_t Started, SnapNext;

/* 0 by day, 1 at night, linear through dusk and dawn */
static float Darkness(void)
{
	int16_t minute = GameTime.getHour() * 60 + GameTime.getMinute();

	if (minute >= DUSK_START && minute < DUSK_START + TWILIGHT_MINUTES)
		return (float) (minute - DUSK_START) / TWILIGHT_MINUTES;
	if (minute >= DAWN_START && minute < DAWN_START + TWILIGHT_MINUTES)
		return 1.0f - (float) (minute - DAWN_START) / TWILIGHT_MINUTES;
	if (minute >= DAWN_START + TWILIGHT_MINUTES && minute < DUSK_START)
		return 0.0f;
	return 1.0f;
}

/* the day as the weather leaves it, as the original's weather cases pick */
static int32_t DayColors(void)
{
	switch (CurrentWeather.type) {
	case 1:
	case 2:
	case 5:
		return OvercastPalette.data;
	case 4:
		return FogPalette.data;
	default:
		return DayPalette.data;
	}
}

static void Blend(float *out, int32_t from, int32_t to, float t)
{
	const uint8_t *a = LINEAR(from), *b = LINEAR(to);

	for (int16_t i = 0; i < COMPONENTS; i++)
		out[i] = a[i] + (b[i] - a[i]) * t;
}

void ScreenPalette::showLighting(uint8_t settled)
{
	float goal[COMPONENTS];
	float darkness = Darkness(), t;
	uint32_t now = plat_milliseconds(), step = now - LastMs;
	uint8_t changed = 0;
	Scene scene;
	int16_t i;
	uint16_t word;

	if (pal == 0 || pal->colors == 0)
		return;
	scene.day = DayColors();
	/* light palettes as the original's light level bands pick them; the spell wins */
	if (state == PAL_LIGHT_SPELL || (lightSpellActive && lightSpellTime != 0))
		scene.dark = LightSpellPalette.data;
	else if (lightLevel == 0)
		scene.dark = NightPalette.data;
	else if (lightLevel < 7)
		scene.dark = CandlePalette.data;
	else if (lightLevel < 20)
		scene.dark = SingleLightPalette.data;
	else
		scene.dark = ManyLightsPalette.data;
	scene.dungeon = InDungeon != 0;
	scene.invisible = state == PAL_INVISIBLE;

	if (scene.invisible)
		Blend(goal, InvisiblePalette.data, InvisiblePalette.data, 0);
	else if (scene.dungeon || darkness >= 1.0f)
		Blend(goal, scene.dark, scene.dark, 0);
	else if (scene.dark != NightPalette.data)
		/* a light keeps dusk away, as the original's did, and takes over as the day goes */
		Blend(goal, scene.day, scene.dark, darkness);
	else if (darkness < 0.5f)
		Blend(goal, scene.day, DuskPalette.data, darkness * 2);
	else
		Blend(goal, DuskPalette.data, scene.dark, darkness * 2 - 1);

	LastMs = now;
	/* Nothing to fade from while the screen is black or nothing was shown yet. A snap asked
	 * for holds until the frame update, once a load or fade-in has placed the avatar. */
	if (!Started || SnapNext || PlayerActionSuspended || fadedOut || !enabled) {
		if (settled) {
			Started = 1;
			SnapNext = 0;
		}
		LastScene = scene;
		FadeMs = FadeLength;
	} else if (LastScene != scene) {
		/* from what shows now, so a change mid-fade turns without a jump */
		memcpy(FadeFrom, Shown, sizeof Shown);
		FadeLength = LastScene.dark == scene.dark && LastScene.dungeon == scene.dungeon
			&& LastScene.invisible == scene.invisible ? WEATHER_FADE_MS : LIGHT_FADE_MS;
		LastScene = scene;
		FadeMs = 0;
	} else if (FadeMs < FadeLength) {
		FadeMs += step < LONGEST_STEP_MS ? step : LONGEST_STEP_MS;
	}

	if (FadeMs >= FadeLength) {
		memcpy(Shown, goal, sizeof Shown);
	} else {
		t = (float) FadeMs / FadeLength;
		t = t * t * (3 - 2 * t);
		for (i = 0; i < COMPONENTS; i++)
			Shown[i] = FadeFrom[i] + (goal[i] - FadeFrom[i]) * t;
	}

	/* words hold the 6-bit value in their high byte */
	for (i = 0; i < COMPONENTS; i++) {
		word = (uint16_t) ((int16_t) (Shown[i] + 0.5f) << 8);
		if (LinearGet16(pal->colors + i * 2) != word) {
			LinearPut16(pal->colors + i * 2, word);
			changed = 1;
		}
	}
	if (changed)
		pal->modified = 1;
}

/* Steps a fade between world draws, once a timer tick, so it runs smoothly; the frame update
 * still decides what to fade to. Called while the game waits for its next draw. */
void ScreenPalette::stepLighting()
{
	if (!Started || FadeMs >= FadeLength || SnapNext || fading || suspended || !enabled || fadedOut
		|| PlayerActionSuspended || plat_ticks() == LastTick)
		return;
	LastTick = plat_ticks();
	showLighting();
	pal->apply(paletteMode);
}

void ScreenPalette::snapLighting()
{
	SnapNext = 1;
}

/* The original's palette changes. With modern lighting the colors come from showLighting,
 * which the frame update calls once it has settled the state. */
static void ShowChange(ScreenPalette *screen)
{
	if (!PaletteFrameUpdate)
		screen->showLighting(0);
	screen->pal->modified = 1;
}

void ScreenPalette::setPalette(int32_t colors)
{
	if (KeepsOriginalLighting()) {
		pal->setColors(colors);
		return;
	}
	/* as setColors, which also undoes the loading screen's color rotation */
	for (int16_t i = 0; i < 256; i++)
		pal->order[i] = i;
	ShowChange(this);
}

void ScreenPalette::creepToPalette(int32_t colors, int16_t speed)
{
	if (KeepsOriginalLighting())
		Creeper_creepTo(this, 1, colors, speed);
	else
		ShowChange(this);
}

extern "C" void ResetLightingGlobals(void)
{
	memset(Shown, 0, sizeof Shown);
	memset(FadeFrom, 0, sizeof FadeFrom);
	memset((void *) &LastScene, 0, sizeof LastScene);
	PaletteFrameUpdate = 0;
	FadeMs = 0;
	FadeLength = LIGHT_FADE_MS;
	LastMs = 0;
	LastTick = 0;
	Started = 0;
	SnapNext = 0;
}
