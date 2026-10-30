/* Process entry, window and threads.
 *
 * The process main thread runs the window (libmatoya wants it there). A launcher thread runs
 * the start menu and the chosen game's launcher in turn (or the game or program named), and each
 * program runs on a game thread of its own. Sound renders on another.
 *
 *   launcher: plat_run_program --reset--> game thread: ProgramMain ... plat_exit(code)
 *                  ^                                                       |
 *                  +------------------- code (thread parked) --------------+
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"

#define WINDOW_TITLE "Ultima VII Modern"

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
static int16_t played_game = 1;
static bool game_named;

/* The window title to show: 0 the program's, 1 Black Gate's, 2 Serpent Isle's. */
static MTY_Atomic32 title;
static int32_t title_shown = -1;

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

/* The fullscreen state last asked of the window; -1 asks again on the next frame. */
static int16_t fullscreen_applied;

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
			if (event->key.pressed) {
				plat_set_setting("fullscreen", !MTY_WindowIsFullscreen(app, window));
				plat_save_settings();
				fullscreen_applied = -1;
			}
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
	/* The setting is applied once the window is up, and again whenever it changes. */
	if (plat_setting("fullscreen") != fullscreen_applied) {
		fullscreen_applied = plat_setting("fullscreen");
		MTY_WindowSetFullscreen(app, window, fullscreen_applied != 0);
	}
	if (MTY_Atomic32Get(&title) != title_shown) {
		static const char *const titles[] = {
			WINDOW_TITLE, WINDOW_TITLE ": Black Gate", WINDOW_TITLE ": Serpent Isle",
		};

		title_shown = MTY_Atomic32Get(&title);
		MTY_WindowSetTitle(app, window, titles[title_shown]);
	}
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
	audio_choose_music();
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

static int16_t run_game(void)
{
	if (played_game == 0)
		return Ultima7Main((int16_t) game_argc, game_argv);
	return SerpentMain((int16_t) game_argc, game_argv);
}

static void *launcher_thread(void *opaque)
{
	int16_t code = 0, choice;

	(void) opaque;
	if (program_name != NULL)
		code = plat_run_program(program_name, (int16_t) (game_argc - 1), game_argv + 1);
	else if (game_named)
		code = run_game();
	else {
		/* The start menu picks a game (1 Black Gate, 2 Serpent Isle); leaving the game comes
		 * back to it, and only quitting it ends the program. */
		for (;;) {
			MTY_Atomic32Set(&title, 0);
			choice = plat_run_program("startmenu", 0, game_argv);
			if (choice != 1 && choice != 2)
				break;
			plat_set_played_game(choice - 1);
			run_game();
		}
	}
	backend_request_exit(code);
	return NULL;
}

int16_t plat_played_game(void)
{
	return played_game;
}

void plat_set_played_game(int16_t game)
{
	played_game = game == 0 ? 0 : 1;
	MTY_Atomic32Set(&title, played_game + 1);
}

int16_t plat_relaxed_npc_pace(void)
{
	return plat_setting("npc_pace") != 0;
}

static void show_help(void)
{
	printf("Usage: Ultima7Modern [--game bg|si] [--bg-data <dir>] [--si-data <dir>] [switches]\n"
		"\n"
		"With no game named, a start menu offers both.\n"
		"\n"
		"  --bg-data <dir>     the Black Gate folder (default: Documents/Ultima7)\n"
		"  --si-data <dir>     the Serpent Isle folder (default: Documents/Serpent)\n"
		"  --data <dir>        the folder of the game named by --game (U7_DATA does the same)\n"
		"  --saves <dir>       where saved games go (default: Saves beside settings.json)\n"
		"  --game <bg|si>      play this game without the start menu: bg Black Gate,\n"
		"                      si Serpent Isle\n"
		"  --npc-pace <relaxed|original>  townsfolk at half speed or as the originals\n"
		"  --aspect <0|1|2>    screen shape for this run: 4:3, 16:10 or 16:9\n"
		"  --program <name>    run one program alone: si or u7 (add p), startmenu, mainmenu,\n"
		"                      intro, endgame; BG helpers: bg-mainmenu, bg-intro, bg-endgame\n"
		"  --help              this list\n"
		"\n"
		"Settings and saves are kept beside the program when both game folders are there, else in\n"
		"the user data folder (U7_USER_DATA overrides both). Here that is:\n"
		"%s\n"
		"Switches change them for one run only.\n"
		"\n"
		"The game's own options:\n"
		"  --cheat             enable the cheat keys\n"
		"  --cheat-start       with --cheat: move at once, move anything, an Avatar that can't\n"
		"                      die, debug output\n"
		"  --speech            speech on\n"
		"  --adlib[=port]      original Adlib music and effects for this run\n"
		"  --opl3              enhanced FM stereo and voices for this run\n"
		"  --roland[=n]        Roland MT-32 music and effects for this run\n"
		"  --shape-pool=<KB>   size of the shape cache\n"
		"  --overlay-size      report the DOS overlay buffer size, then stop\n"
		"  --version           show the version, then stop\n"
		"\n"
		"Settings of this port:\n"
		"  --quiet-weapons     no crackle from the fire sword and firedoom staff (always on)\n"
		"  --mt32-short-waits  no pauses after MT-32 memory writes (always on)\n", settings_data_dir());
	exit(0);
}

/* Takes this backend's switches out of the arguments the game sees. */
static void take_switches(int argc, char **argv, const char **data, const char **bg_data,
	const char **si_data, const char **saves)
{
	game_argv = calloc((size_t) argc + 1, sizeof *game_argv);
	for (int i = 0; i < argc; i++) {
		bool has_value = i > 0 && i + 1 < argc;

		if (i > 0) {
			if (strcmp(argv[i], "--adlib") == 0 || strncmp(argv[i], "--adlib=", 8) == 0) {
				settings_override("sound", 1);
				continue;
			} else if (strcmp(argv[i], "--roland") == 0 || strncmp(argv[i], "--roland=", 9) == 0) {
				settings_override("sound", 2);
				continue;
			} else if (strcmp(argv[i], "--opl3") == 0) {
				settings_override("sound", 3);
				continue;
			}
		}
		if (has_value && strcmp(argv[i], "--data") == 0)
			*data = argv[++i];
		else if (has_value && strcmp(argv[i], "--bg-data") == 0)
			*bg_data = argv[++i];
		else if (has_value && strcmp(argv[i], "--si-data") == 0)
			*si_data = argv[++i];
		else if (has_value && strcmp(argv[i], "--saves") == 0)
			*saves = argv[++i];
		else if (has_value && strcmp(argv[i], "--game") == 0) {
			played_game = strcmp(argv[++i], "bg") == 0 ? 0 : 1;
			game_named = true;
		} else if (has_value && strcmp(argv[i], "--npc-pace") == 0)
			settings_override("npc_pace", strcmp(argv[++i], "original") != 0);
		else if (has_value && strcmp(argv[i], "--aspect") == 0)
			settings_override("aspect", (int16_t) atoi(argv[++i]));
		else if (has_value && strcmp(argv[i], "--program") == 0)
			program_name = argv[++i];
		else if (i > 0 && strcmp(argv[i], "--help") == 0)
			show_help();
		else
			game_argv[game_argc++] = argv[i];
	}
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

/* The folder the program itself is in; for Ultima7Modern.app, the folder holding the app. */
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
	/* ".../Ultima7Modern.app/Contents/MacOS/Ultima7Modern" becomes ".../Ultima7Modern.app". */
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

/* The named folder in the user's Documents folder. */
static bool documents_directory(const char *name, char *out, size_t size)
{
#ifdef _WIN32
	char docs[MAX_PATH];

	if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs) != S_OK)
		return false;
	return snprintf(out, size, "%s\\%s", docs, name) < (int) size;
#else
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0')
		return false;
	return snprintf(out, size, "%s/Documents/%s", home, name) < (int) size;
#endif
}

/* A game's folder when none is named: Documents/Ultima7 or Documents/Serpent, unless a folder
 * of that name with the game's STATIC folder is next to the program. */
/* The game's folder beside the program, when it holds the game. */
static bool game_beside_program(int16_t game, char *out, size_t size)
{
	char own_dir[1024], unused[2048];

	if (!exe_directory(own_dir, sizeof own_dir))
		return false;
	snprintf(out, size, "%s/%s", own_dir, game == 0 ? "Ultima7" : "Serpent");
	files_set_game_root(game, out);
	return files_check_data(game, unused, sizeof unused);
}

static void default_game_dir(int16_t game, char *out, size_t size)
{
	const char *name = game == 0 ? "Ultima7" : "Serpent";

	if (!game_beside_program(game, out, size) && !documents_directory(name, out, size))
		snprintf(out, size, "%s", name);
}

/* With both games beside the program, saves and settings stay there too. */
static bool program_holds_games(char *out, size_t size)
{
	char unused[1100];

	return game_beside_program(0, unused, sizeof unused)
		&& game_beside_program(1, unused, sizeof unused) && exe_directory(out, size);
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
	char problem[4096] = "", message[2048], dirs[2][1024], local[1024];
	const char *data = getenv("U7_DATA"), *bg_data = NULL, *si_data = NULL, *saves = NULL;

	use_parent_console();
	on_app_thread = true;
#ifdef U7_RESET_CHECK
	reset_check_startup();
#endif
	start_time = MTY_GetTime();
	settings_load(program_holds_games(local, sizeof local) ? local : NULL);
	take_switches(argc, argv, &data, &bg_data, &si_data, &saves);
	if (data != NULL && data[0] == '\0')
		data = NULL;
	if (data != NULL)
		*(played_game == 0 ? &bg_data : &si_data) = data;
	for (int16_t game = 0; game < 2; game++) {
		const char *named = game == 0 ? bg_data : si_data;

		if (named != NULL)
			snprintf(dirs[game], sizeof dirs[game], "%s", named);
		else
			default_game_dir(game, dirs[game], sizeof dirs[game]);
		files_set_game_root(game, dirs[game]);
		/* Both games are needed; say what is wrong with each. */
		if (!files_check_data(game, message, sizeof message)) {
			snprintf(problem + strlen(problem), sizeof problem - strlen(problem), "%s%s",
				problem[0] != '\0' ? "\n\n" : "", message);
		}
	}
	if (problem[0] != '\0')
		plat_fatal(problem);
	files_set_saves(saves != NULL ? saves : MTY_JoinPath(settings_data_dir(), "Saves"));
	if (game_named || program_name != NULL)
		plat_set_played_game(played_game);
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
	frame = MTY_MakeDefaultFrame(0, 0, 3 * (uint32_t) plat_video_aspect_width(), 3 * 240, 0.9f);
	window = MTY_WindowCreate(app, WINDOW_TITLE, &frame, 0);
	MTY_AppSetPNGCursor(app, blank_cursor, sizeof blank_cursor, 0, 0);
	MTY_AppSetCursor(app, MTY_CURSOR_ARROW);
	if (window < 0 || !MTY_WindowSetGFX(app, window, MTY_GetDefaultGFX(), true))
		plat_fatal("Could not open the game window.");
	MTY_WindowSetMinSize(app, window, (uint32_t) plat_video_aspect_width(), 240);

	audio_start();
	MTY_ThreadDetach(launcher_thread, NULL);
	MTY_AppRun(app);
	end_process(MTY_Atomic32Get(&exit_code));
	return 0;
}
