/* The indexed screen, 320, 384 or 432 by 200, drawn with pixels 5:6 in the window, or the 80x25
 * text screen. Every frame is shown in a box as wide as the aspect setting's width, so a
 * narrower program is pillarboxed inside it.
 *
 * The game thread converts each presented frame to RGBA with the palette; the app thread
 * draws the newest converted frame every display refresh. Text is drawn by the app thread from
 * the cells the console hands over, so the cursor and blinking characters blink.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "backend.h"
#include "vgafont16.h"

#define WIDTH PLAT_SCREEN_WIDTH
#define MAX_WIDTH PLAT_SCREEN_MAX_WIDTH
#define HEIGHT PLAT_SCREEN_HEIGHT
#define MAX_PIXELS (MAX_WIDTH * HEIGHT)
/* 200 lines shown 240 tall. */
#define ASPECT_HEIGHT 240
#define RETRACE_HZ 70.086
/* yield shows the frame again no more often than this. */
#define REFRESH_US 8000
/* VGA text: 9x16 cells, 720x400. */
#define TEXT_COLUMNS 80
#define TEXT_ROWS 25
#define TEXT_WIDTH (TEXT_COLUMNS * 9)
#define TEXT_HEIGHT (TEXT_ROWS * 16)
#define TEXT_PIXELS (TEXT_WIDTH * TEXT_HEIGHT)

/* Game thread. */
static uint8_t palette[256][4];
static bool palette_dirty;
static const uint8_t *last_pixels;
static int64_t last_present_us;
static int16_t frame_width = WIDTH;

/* Handed to the app thread under frame_lock. */
static MTY_Mutex *frame_lock;
static uint8_t shared_rgba[MAX_PIXELS][4];
static bool shared_new;
/* The image's width (the aspect setting's), the frame's width centred in it, and the filter. */
static int32_t shared_width = WIDTH;
static int32_t shared_frame_width = WIDTH;
static int16_t shared_filter;
static bool shared_text;
static uint16_t shared_cells[TEXT_COLUMNS * TEXT_ROWS];
static int16_t shared_cursor_x, shared_cursor_y;

/* Game thread: the game's frames are not on show in text mode. */
static bool text_shown;

/* App thread. */
static uint8_t draw_rgba[TEXT_PIXELS][4];
void video_init(void)
{
	frame_lock = MTY_MutexCreate();
	for (int i = 0; i < 256; i++)
		palette[i][3] = 0xFF;
	for (int i = 0; i < TEXT_PIXELS; i++)
		draw_rgba[i][3] = 0xFF;
}

/* A new program starts with a black palette and no frame of its own; the window keeps
 * showing the last one until it presents. */
void video_reset(void)
{
	for (int i = 0; i < 256; i++)
		memset(palette[i], 0, 3);
	palette_dirty = false;
	last_pixels = NULL;
	last_present_us = 0;
	frame_width = WIDTH;
	text_shown = false;
	MTY_MutexLock(frame_lock);
	shared_frame_width = WIDTH;
	shared_text = false;
	MTY_MutexUnlock(frame_lock);
}

void plat_video_set_palette(int16_t first, int16_t count, const uint8_t *rgb)
{
	for (int16_t i = 0; i < count; i++) {
		int index = first + i;

		if (index < 0 || index > 255)
			continue;
		for (int c = 0; c < 3; c++) {
			uint8_t v = rgb[3 * i + c] & 0x3F;

			palette[index][c] = (uint8_t) (v << 2 | v >> 4);
		}
	}
	palette_dirty = true;
}

int16_t plat_video_aspect_width(void)
{
	static const int16_t widths[] = {320, 384, 432};
	int16_t aspect = plat_setting("aspect");

	return widths[aspect >= 0 && aspect <= 2 ? aspect : 0];
}

void plat_video_set_width(int16_t width)
{
	frame_width = width;
	MTY_MutexLock(frame_lock);
	shared_frame_width = width;
	MTY_MutexUnlock(frame_lock);
}

static void convert(const uint8_t *pixels)
{
	static const uint8_t black[4] = {0, 0, 0, 0xFF};
	int32_t width, left;

	if (text_shown)
		return;
	width = plat_video_aspect_width();
	if (width < frame_width)
		width = frame_width;
	left = (width - frame_width) / 2;
	MTY_MutexLock(frame_lock);
	shared_width = width;
	shared_frame_width = frame_width;
	shared_filter = plat_setting("filter");
	for (int y = 0; y < HEIGHT; y++) {
		uint8_t (*out)[4] = shared_rgba + y * width;

		for (int x = 0; x < width; x++) {
			int32_t column = x - left;

			if (column >= 0 && column < frame_width)
				memcpy(out[x], palette[pixels[y * frame_width + column]], 4);
			else
				memcpy(out[x], black, 4);
		}
	}
	shared_new = true;
	MTY_MutexUnlock(frame_lock);
	palette_dirty = false;
	last_present_us = backend_elapsed_us();
}

void plat_video_present(const uint8_t *pixels)
{
	last_pixels = pixels;
	convert(pixels);
	test_input_frame();
}

uint8_t *video_screen(void)
{
	return (uint8_t *) last_pixels;
}

void video_refresh(bool force)
{
	if (last_pixels == NULL)
		return;
	if (force || palette_dirty || backend_elapsed_us() - last_present_us >= REFRESH_US)
		convert(last_pixels);
}

void video_show_text(const uint16_t *cells, int16_t cursor_x, int16_t cursor_y)
{
	text_shown = true;
	MTY_MutexLock(frame_lock);
	memcpy(shared_cells, cells, sizeof shared_cells);
	shared_cursor_x = cursor_x;
	shared_cursor_y = cursor_y;
	shared_text = true;
	MTY_MutexUnlock(frame_lock);
}

void video_show_game(void)
{
	text_shown = false;
	MTY_MutexLock(frame_lock);
	shared_text = false;
	shared_new = true;
	MTY_MutexUnlock(frame_lock);
	video_refresh(true);
}

/* The VGA text palette: attribute colours 0-15. */
static const uint8_t text_colours[16][3] = {
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0xAA}, {0x00, 0xAA, 0x00}, {0x00, 0xAA, 0xAA},
	{0xAA, 0x00, 0x00}, {0xAA, 0x00, 0xAA}, {0xAA, 0x55, 0x00}, {0xAA, 0xAA, 0xAA},
	{0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
	{0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF},
};

/* As VGA mode 3 drew it: the ninth pixel column repeats the eighth for the line-drawing
 * characters C0-DF, attribute bit 7 blinks the character every 16 frames, and the cursor
 * (scan lines 13-14, in the character's colour) blinks every 8. */
void video_render_text(uint8_t (*rgba)[4], const uint16_t *cells, int16_t cursor_x,
	int16_t cursor_y, int64_t frame)
{
	bool blink_on = (frame >> 4) & 1;
	bool cursor_on = (frame >> 3) & 1;

	for (int row = 0; row < TEXT_ROWS; row++) {
		for (int col = 0; col < TEXT_COLUMNS; col++) {
			uint8_t ch = cells[row * TEXT_COLUMNS + col] & 0xFF;
			uint8_t attribute = cells[row * TEXT_COLUMNS + col] >> 8;
			const uint8_t *fg = text_colours[attribute & 15];
			const uint8_t *bg = text_colours[(attribute >> 4) & 7];
			bool hidden = (attribute & 0x80) && !blink_on;
			bool cursor = cursor_on && row == cursor_y && col == cursor_x;

			for (int y = 0; y < 16; y++) {
				uint8_t bits = hidden ? 0 : VgaFont16[ch][y];
				uint8_t (*out)[4] = rgba + (row * 16 + y) * TEXT_WIDTH + col * 9;

				if (cursor && (y == 13 || y == 14))
					bits = 0xFF;
				for (int x = 0; x < 9; x++) {
					bool lit = x < 8 ? bits & (0x80 >> x) : ch >= 0xC0 && ch <= 0xDF && (bits & 1);

					memcpy(out[x], lit ? fg : bg, 3);
					out[x][3] = 0xFF;
				}
			}
		}
	}
}

void plat_video_wait_retrace(void)
{
	double period_us = 1000000.0 / RETRACE_HZ;
	int64_t now, next;

	plat_pump();
	/* On VGA a palette write showed at once; fades rely on that. */
	if (palette_dirty)
		video_refresh(true);
	now = backend_elapsed_us();
	next = (int64_t) ((floor((double) now / period_us) + 1.0) * period_us);
	/* Rounding up lands past the boundary, so the next wait finds a later one. */
	backend_sleep((uint32_t) ((next - now + 999) / 1000));
}

static uint32_t integer_scale(MTY_Size view, int32_t width)
{
	uint32_t sx = view.w / (uint32_t) width;
	uint32_t sy = view.h / ASPECT_HEIGHT;

	return sx < sy ? sx : sy;
}

/* Pixel-exact scaling draws at a whole-number scale; filling (the default) uses the largest fit. */
static uint32_t pixel_exact_scale(MTY_Size view, int32_t width)
{
	return shared_filter == 1 ? integer_scale(view, width) : 0;
}

/* The whole image's box in the window, centred: a whole-number scale when pixel-exact and one
 * fits, else the largest fit. Drawing, the mouse and pointer warps all go through this. */
static void image_rect(MTY_Size view, int32_t width, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
	uint32_t s = pixel_exact_scale(view, width);

	if (s > 0) {
		*w = (int32_t) (width * s);
		*h = (int32_t) (ASPECT_HEIGHT * s);
	} else if ((uint64_t) view.w * ASPECT_HEIGHT >= (uint64_t) view.h * width) {
		*h = (int32_t) view.h;
		*w = (int32_t) ((uint64_t) view.h * width / ASPECT_HEIGHT);
	} else {
		*w = (int32_t) view.w;
		*h = (int32_t) ((uint64_t) view.w * ASPECT_HEIGHT / width);
	}
	*x = ((int32_t) view.w - *w) / 2;
	*y = ((int32_t) view.h - *h) / 2;
}

void video_view_rect(MTY_Size view, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
	int32_t width, frame, left;

	MTY_MutexLock(frame_lock);
	width = shared_width;
	frame = shared_frame_width;
	MTY_MutexUnlock(frame_lock);
	image_rect(view, width, x, y, w, h);
	left = *w * ((width - frame) / 2) / width;
	*x += left;
	*w = *w * frame / width;
}

int16_t video_mouse_width(void)
{
	int16_t width;

	/* static constructors move the mouse before video_init */
	if (frame_lock == NULL)
		return 2 * WIDTH;
	MTY_MutexLock(frame_lock);
	width = (int16_t) (2 * shared_frame_width);
	MTY_MutexUnlock(frame_lock);
	return width;
}

void video_draw(MTY_App *app, MTY_Window window)
{
	MTY_Size view = MTY_WindowGetSize(app, window);
	MTY_RenderDesc desc = {
		.format = MTY_COLOR_FORMAT_RGBA,
		.filter = MTY_FILTER_NEAREST,
		.imageHeight = HEIGHT,
		.cropHeight = HEIGHT,
	};
	uint32_t s;

	MTY_MutexLock(frame_lock);
	s = pixel_exact_scale(view, shared_width);
	desc.imageWidth = desc.cropWidth = (uint32_t) shared_width;
	desc.aspectRatio = (float) shared_width / ASPECT_HEIGHT;
	desc.scale = (float) s;
	/* Nearest at whole scales; sharpened bilinear when filling. */
	if (s == 0) {
		desc.filter = MTY_FILTER_LINEAR;
		desc.effects[0] = MTY_EFFECT_SHARPEN;
		desc.levels[0] = 1.0f;
	}
	if (shared_text) {
		video_render_text(draw_rgba, shared_cells, shared_cursor_x, shared_cursor_y,
			(int64_t) (backend_elapsed_us() * RETRACE_HZ / 1000000.0));
		/* 720x400 fills the same 4:3 area as 320x200 */
		desc.imageWidth = desc.cropWidth = TEXT_WIDTH;
		desc.imageHeight = desc.cropHeight = TEXT_HEIGHT;
		desc.aspectRatio = (float) WIDTH / ASPECT_HEIGHT;
		desc.scale = desc.scale * WIDTH / TEXT_WIDTH;
		shared_new = true;
	} else if (shared_new) {
		memcpy(draw_rgba, shared_rgba, (size_t) shared_width * HEIGHT * 4);
		shared_new = false;
	}
	MTY_MutexUnlock(frame_lock);

	MTY_WindowDrawQuad(app, window, draw_rgba, &desc);
	MTY_WindowPresent(app, window);
}

int16_t plat_cursor_is_hardware(void)
{
	return 0;
}

void plat_cursor_set_shape(const uint8_t *pixels, int16_t width, int16_t height,
	int16_t hot_x, int16_t hot_y)
{
	(void) pixels;
	(void) width;
	(void) height;
	(void) hot_x;
	(void) hot_y;
}

void plat_cursor_show(int16_t visible)
{
	(void) visible;
}

bool video_write_shot(const char *path)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL)
		return false;
	MTY_MutexLock(frame_lock);
	if (shared_text) {
		static uint8_t text_rgba[TEXT_PIXELS][4];

		/* the frame count picks the phase with the cursor and blinking characters shown */
		video_render_text(text_rgba, shared_cells, shared_cursor_x, shared_cursor_y, 24);
		fprintf(f, "P6\n%d %d\n255\n", TEXT_WIDTH, TEXT_HEIGHT);
		for (int i = 0; i < TEXT_PIXELS; i++)
			fwrite(text_rgba[i], 1, 3, f);
	} else {
		fprintf(f, "P6\n%d %d\n255\n", (int) shared_width, HEIGHT);
		for (int i = 0; i < shared_width * HEIGHT; i++)
			fwrite(shared_rgba[i], 1, 3, f);
	}
	MTY_MutexUnlock(frame_lock);
	fclose(f);
	return true;
}
