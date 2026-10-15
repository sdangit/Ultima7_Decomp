/* Process entry, window and threads.
 *
 * The process main thread runs the window (libmatoya wants it there). A launcher thread runs
 * SerpentMain, or the one program named, and each program runs on a game thread of its own.
 * Sound renders on another.
 *
 *   launcher: plat_run_program --reset--> game thread: ProgramMain ... plat_exit(code)
 *                  ^                                                       |
 *                  +------------------- code (thread parked) --------------+
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "settings.h"
#ifdef __APPLE__
#include "displays.h"
#endif

#define WINDOW_TITLE "Ultima VII Part Two: Serpent Isle"

static MTY_App *app;
static MTY_Window window = -1;
static MTY_Time start_time;
static _Thread_local bool on_app_thread;
static _Thread_local bool on_game_thread;

static MTY_Atomic32 exit_claimed;
static MTY_Atomic32 exit_requested;
static MTY_Atomic32 exit_code;
static MTY_Atomic32 fatal_claimed;
static MTY_Atomic32 fatal_pending;
static char fatal_text[1024];

static int game_argc;
static char **game_argv;
static const char *program_name;
/* Initial window size, in multiples of the aspect-corrected 320x240 image. */
static uint32_t window_scale = 3;
static bool list_displays;
static bool display_override;
static bool save_settings;
static const char *settings_path;
static U7_Settings cli_settings;
static uint32_t cli_settings_mask;

/* A program's exit code, handed to the launcher waiting in plat_run_program. */
static MTY_Waitable *program_done;
static int16_t program_code;

/* The window's client size, for mapping scripted mouse positions on the game thread. */
static MTY_Mutex *view_lock;
static MTY_Size view_size = {3 * 320, 3 * 240};

MTY_Size backend_view_size(void)
{
	MTY_Size size;

	MTY_MutexLock(view_lock);
	size = view_size;
	MTY_MutexUnlock(view_lock);
	return size;
}

int64_t backend_elapsed_us(void)
{
	if (test_clock_fixed())
		return test_clock_us();
	return (int64_t) (MTY_TimeDiff(start_time, MTY_GetTime()) * 1000.0);
}

void backend_request_exit(int code)
{
	if (MTY_Atomic32CAS(&exit_claimed, 0, 1)) {
		MTY_Atomic32Set(&exit_code, code);
		MTY_Atomic32Set(&exit_requested, 1);
	}
}

/* A thread that asked the app thread to end the process waits here for it. */
static void wait_for_exit(void)
{
	for (;;)
		MTY_Sleep(1000);
}

/* Ends at once, as a DOS program did. The system frees the window, the sound device and the
 * memory; tearing them down first only adds delay and risk (an X11 display closed under a live
 * GL context can corrupt the driver's heap). Static destructors are skipped too: the game thread
 * may still be running. */
static void end_process(int code)
{
	fflush(NULL);
	_Exit(code);
}

void plat_exit(int16_t code)
{
	if (on_app_thread)
		end_process(code);
	if (on_game_thread) {
		/* As a DOS exit: the program's stack is left as it is, never unwound. */
		audio_drop_lock();
		program_code = (int16_t) (code & 0xff);
		MTY_WaitableSignal(program_done);
		wait_for_exit();
	}
	backend_request_exit(code);
	wait_for_exit();
}

void plat_fatal(const char *message)
{
	fprintf(stderr, "u7: %s\n", message);
#ifdef U7_ARENA_CHECK
	arena_check_trace("fatal error");
#endif
	if (on_app_thread) {
		/* A scripted run has no one to close the box. */
		if (MTY_HasDialogs() && !test_input_active())
			MTY_ShowMessageBox(WINDOW_TITLE, "%s", message);
		end_process(1);
	}
	/* Message boxes must be shown from the app thread. */
	if (MTY_Atomic32CAS(&fatal_claimed, 0, 1)) {
		snprintf(fatal_text, sizeof fatal_text, "%s", message);
		MTY_Atomic32Set(&fatal_pending, 1);
	}
	wait_for_exit();
}

void plat_log(const char *text)
{
	fputs(text, stderr);
	fflush(stderr);
}

static bool point_over_image(int32_t x, int32_t y)
{
	int32_t vx, vy, vw, vh;

	video_view_rect(MTY_WindowGetSize(app, window), &vx, &vy, &vw, &vh);
	return x >= vx && y >= vy && x < vx + vw && y < vy + vh;
}

/* A fully transparent 1x1 PNG. Over the game image the OS pointer is this, since the game
 * draws its own; outside the window macOS shows its normal arrow by itself. */
static const uint8_t blank_cursor[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
	0x0b, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x60, 0x00, 0x02, 0x00,
	0x00, 0x05, 0x00, 0x01, 0x7a, 0x5e, 0xab, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static bool pointer_over_image;

static void on_event(const MTY_Event *event, void *opaque)
{
	(void) opaque;

	switch (event->type) {
	case MTY_EVENT_CLOSE:
	case MTY_EVENT_QUIT:
	case MTY_EVENT_SHUTDOWN:
		backend_request_exit(0);
		return;
	case MTY_EVENT_KEY:
		/* Alt-Enter toggles fullscreen and never reaches the game. */
		if ((event->key.key == MTY_KEY_ENTER || event->key.key == MTY_KEY_NP_ENTER)
			&& (event->key.mod & MTY_MOD_ALT)) {
			if (event->key.pressed)
				MTY_WindowSetFullscreen(app, window, !MTY_WindowIsFullscreen(app, window));
			return;
		}
		break;
	case MTY_EVENT_MOTION:
		/* The game draws its own pointer over the image. */
		if (!event->motion.relative) {
			bool over = point_over_image(event->motion.x, event->motion.y);

			if (over != pointer_over_image) {
				pointer_over_image = over;
				MTY_AppSetCursor(app, over ? MTY_CURSOR_NONE : MTY_CURSOR_ARROW);
			}
		}
		break;
	default:
		break;
	}
	events_push(event, MTY_WindowGetSize(app, window));
}

/* Scripted runs open no window, so nothing takes focus or switches Spaces and no display is
 * needed. Frames still reach video's shared buffer, where "shot" reads them. */
static void run_without_window(void)
{
	while (!MTY_Atomic32Get(&exit_requested)) {
		if (MTY_Atomic32Get(&fatal_pending)) {
			MTY_Atomic32Set(&exit_code, 1);
			break;
		}
		MTY_Sleep(16);
	}
	end_process(MTY_Atomic32Get(&exit_code));
}

static bool on_frame(void *opaque)
{
	uint32_t x, y;

	(void) opaque;
	if (MTY_Atomic32Get(&fatal_pending)) {
		if (MTY_HasDialogs() && !test_input_active())
			MTY_ShowMessageBox(WINDOW_TITLE, "%s", fatal_text);
		MTY_Atomic32Set(&exit_code, 1);
		return false;
	}
	if (MTY_Atomic32Get(&exit_requested))
		return false;
	MTY_MutexLock(view_lock);
	view_size = MTY_WindowGetSize(app, window);
	MTY_MutexUnlock(view_lock);
	if (events_take_warp(MTY_WindowGetSize(app, window), &x, &y))
		MTY_WindowWarpCursor(app, window, x, y);
	video_draw(app, window);
	/* Presenting does not wait for a display refresh while the window is hidden. */
	if (!MTY_WindowIsVisible(app, window))
		MTY_Sleep(16);
	return true;
}

typedef struct {
	const char *name;
	int16_t argc;
	char **argv;
} program_start;

static void *game_thread(void *opaque)
{
	program_start *start = opaque;

	on_game_thread = true;
	plat_exit(ProgramMain(start->name, start->argc, start->argv));
	return NULL;
}

int16_t plat_run_program(const char *name, int16_t argc, char **argv)
{
	program_start start = {name, (int16_t) (argc + 1), calloc((size_t) argc + 2, sizeof (char *))};

	/* The program sees this executable as its argv[0], as a DOS program saw its EXE. */
	start.argv[0] = game_argv[0];
	memcpy(start.argv + 1, argv, (size_t) argc * sizeof *argv);

	events_reset();
	video_reset();
	audio_reset();
	files_reset();
	console_reset();
	nullpage_reset();
	ResetEnvironment();
#ifdef U7_RESET_CHECK
	reset_check();
#endif
	MTY_ThreadDetach(game_thread, &start);
	MTY_WaitableWait(program_done, -1);
	free(start.argv);
	return program_code;
}

static void *launcher_thread(void *opaque)
{
	int16_t code;

	(void) opaque;
	if (program_name == NULL)
		code = SerpentMain((int16_t) game_argc, game_argv);
	else
		code = plat_run_program(program_name, (int16_t) (game_argc - 1), game_argv + 1);
	backend_request_exit(code);
	return NULL;
}

static void show_help(void)
{
	printf("Usage: Serpent [--data <dir>] [switches]\n"
		"\n"
		"  --data <dir>        the game folder (default: U7_DATA, else the current folder if it\n"
		"                      has STATIC, else the folder Serpent is in)\n"
		"  --settings <file>   settings file (default: settings.cfg in the game folder)\n"
		"  --save-settings     write effective settings before launching (otherwise CLI is temporary)\n"
		"  --program <name>    run one program alone: si (add p), mainmenu, intro, endgame\n"
		"  --help              this list\n"
		"\n"
		"The game's own options:\n"
		"  --cheat             enable the cheat keys\n"
		"  --cheat-start       with --cheat: move at once, move anything, an Avatar that can't\n"
		"                      die, debug output\n"
		"  --speech            speech on\n"
		"  --adlib[=port]      AdLib music, even when MT-32 ROMs are found\n"
		"  --roland[=n]        Roland MT-32 music (the default when its ROMs are found)\n"
		"  --shape-pool=<KB>   size of the shape cache\n"
		"  --overlay-size      report the DOS overlay buffer size, then stop\n"
		"  --version           show the version, then stop\n"
		"\n"
		"Settings of this port:\n"
		"  --scale=<N>        initial window size: N times 320x240 (default: 3); reduced to fit\n"
		"                      the screen; macOS uses points, before Retina scaling\n"
		"  --display=<N>      macOS: display number, primary, or uuid:UUID (default: primary)\n"
		"  --list-displays    macOS: list connected displays and their sizes, then exit\n"
		"  --scanlines[=N]    scanline darkness, 0 to 1 (bare flag: 0.3; default: off)\n"
		"  --music-device=M  auto, adlib, or roland; --adlib/--roland also override this setting\n"
		"  --music-volume=N  combined music/SFX volume, 0 to 100 (default: 100)\n"
		"  --speech-volume=N speech volume, 0 to 100 (default: 100)\n"
		"  --quiet-weapons     no crackle from the fire sword and firedoom staff\n"
		"  --no-quiet-weapons override quiet_weapons=true in settings\n"
		"  --mt32-short-waits  no pauses after MT-32 memory writes (timbre uploads), which\n"
		"                      the emulated synth does not need\n"
		"  --no-mt32-short-waits override mt32_short_waits=true in settings\n");
	exit(0);
}

static void set_cli_setting(const char *key, const char *value)
{
	char error[384];

	if (!settings_set(&cli_settings, &cli_settings_mask, key, value, error, sizeof error)) {
		fprintf(stderr, "u7: command line: %s\n", error);
		exit(2);
	}
}

static const char *settings_file_argument(const char *value)
{
	if (value == NULL || value[0] == '\0' || strncmp(value, "--", 2) == 0) {
		fprintf(stderr, "u7: --settings requires a file path\n");
		exit(2);
	}
	return value;
}

/* Consume backend options before forwarding the remaining arguments to the game. */
static const char *take_data_dir(int argc, char **argv)
{
	const char *dir = getenv("U7_DATA");

	settings_defaults(&cli_settings);
	/* Two saved boolean preferences may add game arguments even with no CLI arguments. */
	game_argv = calloc((size_t) argc + 3, sizeof *game_argv);
	for (int i = 0; i < argc; i++) {
		if (i > 0 && strcmp(argv[i], "--data") == 0 && i + 1 < argc)
			dir = argv[++i];
		else if (i > 0 && strcmp(argv[i], "--program") == 0 && i + 1 < argc)
			program_name = argv[++i];
		else if (i > 0 && strcmp(argv[i], "--settings") == 0)
			settings_path = settings_file_argument(i + 1 < argc ? argv[++i] : NULL);
		else if (i > 0 && strncmp(argv[i], "--settings=", 11) == 0)
			settings_path = settings_file_argument(argv[i] + 11);
		else if (i > 0 && strcmp(argv[i], "--save-settings") == 0)
			save_settings = true;
		else if (i > 0 && strcmp(argv[i], "--scale") == 0)
			set_cli_setting("scale", i + 1 < argc ? argv[++i] : NULL);
		else if (i > 0 && strncmp(argv[i], "--scale=", 8) == 0)
			set_cli_setting("scale", argv[i] + 8);
		else if (i > 0 && strcmp(argv[i], "--display") == 0) {
			set_cli_setting("display", i + 1 < argc ? argv[++i] : NULL);
			display_override = true;
		} else if (i > 0 && strncmp(argv[i], "--display=", 10) == 0) {
			set_cli_setting("display", argv[i] + 10);
			display_override = true;
		} else if (i > 0 && strcmp(argv[i], "--list-displays") == 0)
			list_displays = true;
		else if (i > 0 && strcmp(argv[i], "--scanlines") == 0)
			set_cli_setting("scanlines", "0.3");
		else if (i > 0 && strncmp(argv[i], "--scanlines=", 12) == 0)
			set_cli_setting("scanlines", argv[i] + 12);
		else if (i > 0 && strcmp(argv[i], "--music-volume") == 0)
			set_cli_setting("music_volume", i + 1 < argc ? argv[++i] : NULL);
		else if (i > 0 && strncmp(argv[i], "--music-volume=", 15) == 0)
			set_cli_setting("music_volume", argv[i] + 15);
		else if (i > 0 && strcmp(argv[i], "--speech-volume") == 0)
			set_cli_setting("speech_volume", i + 1 < argc ? argv[++i] : NULL);
		else if (i > 0 && strncmp(argv[i], "--speech-volume=", 16) == 0)
			set_cli_setting("speech_volume", argv[i] + 16);
		else if (i > 0 && strcmp(argv[i], "--music-device") == 0)
			set_cli_setting("music_device", i + 1 < argc ? argv[++i] : NULL);
		else if (i > 0 && strncmp(argv[i], "--music-device=", 15) == 0)
			set_cli_setting("music_device", argv[i] + 15);
		else if (i > 0 && strcmp(argv[i], "--quiet-weapons") == 0)
			set_cli_setting("quiet_weapons", "true");
		else if (i > 0 && strcmp(argv[i], "--no-quiet-weapons") == 0)
			set_cli_setting("quiet_weapons", "false");
		else if (i > 0 && strcmp(argv[i], "--mt32-short-waits") == 0)
			set_cli_setting("mt32_short_waits", "true");
		else if (i > 0 && strcmp(argv[i], "--no-mt32-short-waits") == 0)
			set_cli_setting("mt32_short_waits", "false");
		else if (i > 0 && strcmp(argv[i], "--help") == 0)
			show_help();
		else {
			/* Retain the game's optional port arguments while recording the backend choice. */
			if (strcmp(argv[i], "--adlib") == 0 || strncmp(argv[i], "--adlib=", 8) == 0)
				set_cli_setting("music_device", "adlib");
			else if (strcmp(argv[i], "--roland") == 0 || strncmp(argv[i], "--roland=", 9) == 0)
				set_cli_setting("music_device", "roland");
			game_argv[game_argc++] = argv[i];
		}
	}
	return dir != NULL && dir[0] != '\0' ? dir : NULL;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

/* The folder the program itself is in; for Serpent.app, the folder holding the app. */
static bool exe_directory(char *out, size_t size)
{
	char *slash;

#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, out, (DWORD) size);

	if (n == 0 || n >= size)
		return false;
	slash = strrchr(out, '\\');
#elif defined(__APPLE__)
	uint32_t n = (uint32_t) size;

	char *bundle;

	if (_NSGetExecutablePath(out, &n) != 0)
		return false;
	/* ".../Serpent.app/Contents/MacOS/Serpent" becomes ".../Serpent.app". */
	bundle = strstr(out, ".app/Contents/MacOS/");
	if (bundle != NULL)
		bundle[4] = '\0';
	slash = strrchr(out, '/');
#else
	ssize_t n = readlink("/proc/self/exe", out, size - 1);

	if (n <= 0)
		return false;
	out[n] = '\0';
	slash = strrchr(out, '/');
#endif
	if (slash == NULL)
		return false;
	*slash = '\0';
	return true;
}

/* Serpent in the user's Documents folder. */
static bool documents_directory(char *out, size_t size)
{
#ifdef _WIN32
	char docs[MAX_PATH];

	if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs) != S_OK)
		return false;
	return snprintf(out, size, "%s\\Serpent", docs) < (int) size;
#else
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0')
		return false;
	return snprintf(out, size, "%s/Documents/Serpent", home) < (int) size;
#endif
}

/* Called after resolving and checking the game folder, before starting any game/audio thread. */
static bool configure_settings(MTY_Frame *frame)
{
	char default_path[4096], error[4608];
	const char *path = settings_path;
	U7_Settings settings;
	bool missing;

	if (path == NULL) {
		if (snprintf(default_path, sizeof default_path, "%s/settings.cfg", files_root()) >= (int) sizeof default_path) {
			fprintf(stderr, "u7: game folder path is too long for settings.cfg\n");
			return false;
		}
		path = default_path;
	}
	settings_defaults(&settings);
	if (!settings_load(path, &settings, &missing, error, sizeof error)) {
		fprintf(stderr, "u7: %s\n", error);
		return false;
	}
	settings_overlay(&settings, &cli_settings, cli_settings_mask);
#ifdef __APPLE__
	/* A saved monitor may have been unplugged. Explicit requests must still resolve. */
	if (!test_input_active() || display_override
		|| (save_settings && strcmp(settings.display, "primary") != 0)) {
		uint32_t display;

		if (!displays_resolve(settings.display, !display_override, &display)
			|| !displays_window_frame(display, settings.scale, frame))
			return false;
		/* Keep a disconnected saved UUID so reconnecting restores the requested monitor. */
		if (save_settings && strcmp(settings.display, "primary") != 0 && display != 0
			&& !displays_persistent_name(display, settings.display, sizeof settings.display)) {
			fprintf(stderr, "u7: could not obtain a persistent display identifier\n");
			return false;
		}
	}
#else
	(void) frame;
#endif
	if (missing || save_settings) {
		U7_Settings written = settings;

		/* First-run CLI experiments do not become preferences without --save-settings. */
		if (!save_settings) settings_defaults(&written);
		if (!settings_write(path, &written, !missing, error, sizeof error)) {
			fprintf(stderr, "u7: %s\n", error);
			return false;
		}
	}
	/* A later --music-device override supersedes earlier device switches, including their
	 * optional game arguments. Keep optional arguments only for the effective device. */
	int forwarded = 0;
	for (int i = 0; i < game_argc; i++) {
		bool adlib = strcmp(game_argv[i], "--adlib") == 0 || strncmp(game_argv[i], "--adlib=", 8) == 0;
		bool roland = strcmp(game_argv[i], "--roland") == 0 || strncmp(game_argv[i], "--roland=", 9) == 0;

		if (i > 0 && ((adlib && settings.music_device != 'a') || (roland && settings.music_device != 'r')))
			continue;
		game_argv[forwarded++] = game_argv[i];
	}
	game_argc = forwarded;
	game_argv[game_argc] = NULL;
	window_scale = settings.scale;
	video_set_scanlines(settings.scanlines);
	audio_set_music_volume(settings.music_volume);
	audio_set_speech_volume(settings.speech_volume);
	audio_choose_music(settings.music_device);
	if (settings.quiet_weapons) game_argv[game_argc++] = "--quiet-weapons";
	if (settings.mt32_short_waits) game_argv[game_argc++] = "--mt32-short-waits";
	game_argv[game_argc] = NULL;
	return true;
}

#ifdef _WIN32

/* The exe is a windowed program, so a double-click opens no console. Started from a terminal,
 * its messages go to that terminal; output already redirected to a file or pipe stays there. */
static void use_parent_console(void)
{
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return;
	if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN)
		freopen("CONOUT$", "w", stdout);
	if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN)
		freopen("CONOUT$", "w", stderr);
}
#else
static void use_parent_console(void)
{
}
#endif

int main(int argc, char **argv)
{
	MTY_Frame frame;
	char problem[1024], own_dir[1024] = "", docs_dir[1024];
	const char *data_dir;

	use_parent_console();
	on_app_thread = true;
#ifdef U7_RESET_CHECK
	reset_check_startup();
#endif
	start_time = MTY_GetTime();
	data_dir = take_data_dir(argc, argv);
#ifdef __APPLE__
	if (list_displays)
		return displays_list() ? 0 : 2;
#else
	if (list_displays || display_override) {
		fprintf(stderr, "u7: --display and --list-displays are supported only on macOS\n");
		return 2;
	}
#endif
	files_set_root(data_dir != NULL ? data_dir : ".");
	/* With no folder named: the current one, the program's own (a double-click does not always
	 * start in the game's folder), then Documents/Serpent. Documents comes last because macOS
	 * asks the player before letting an app look there. */
	if (data_dir == NULL && !plat_dir_exists("STATIC")) {
		const char *fallback = exe_directory(own_dir, sizeof own_dir) ? own_dir : ".";

		files_set_root(fallback);
		if (!plat_dir_exists("STATIC") && documents_directory(docs_dir, sizeof docs_dir)) {
			files_set_root(docs_dir);
#ifndef __APPLE__
			/* Found nowhere: report the program's folder, where the game is meant to go. A Mac
			 * install keeps the app in Applications and the game in Documents/Serpent. */
			if (!plat_dir_exists("STATIC"))
				files_set_root(fallback);
#endif
		}
	}
	if (!files_check_data(problem, sizeof problem)) {
		/* macOS runs a downloaded app from a hidden copy until it is moved in Finder. */
		if (strstr(own_dir, "/AppTranslocation/") != NULL)
			snprintf(problem, sizeof problem, "macOS started Serpent from a temporary copy, so "
				"it can't see the game files. In Finder, drag Serpent.app out of the game "
				"folder and back in, then open it again.");
		plat_fatal(problem);
	}
	if (!configure_settings(&frame))
		return 2;
	nullpage_install();
	events_init();
	video_init();
	view_lock = MTY_MutexCreate();
	program_done = MTY_WaitableCreate();

	if (test_input_active()) {
		audio_start();
		MTY_ThreadDetach(launcher_thread, NULL);
		run_without_window();
	}
	app = MTY_AppCreate(0, on_frame, on_event, NULL);
	if (app == NULL)
		plat_fatal("Could not start the window system.");
#ifndef __APPLE__
	frame = MTY_MakeDefaultFrame(0, 0, window_scale * 320, window_scale * 240, 0.9f);
#endif
	window = MTY_WindowCreate(app, WINDOW_TITLE, &frame, 0);
	MTY_AppSetPNGCursor(app, blank_cursor, sizeof blank_cursor, 0, 0);
	MTY_AppSetCursor(app, MTY_CURSOR_ARROW);
	if (window < 0 || !MTY_WindowSetGFX(app, window, MTY_GetDefaultGFX(), true))
		plat_fatal("Could not open the game window.");
	MTY_WindowSetMinSize(app, window, 320, 240);

	audio_start();
	MTY_ThreadDetach(launcher_thread, NULL);
	MTY_AppRun(app);
	end_process(MTY_Atomic32Get(&exit_code));
	return 0;
}
