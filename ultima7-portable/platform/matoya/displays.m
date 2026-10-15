/* libmatoya accepts native display IDs, but its default frame helper always sizes against
 * the primary screen. Select the screen first and fit the whole window in its usable area. */
#import <AppKit/AppKit.h>

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "displays.h"

static uint32_t display_id(NSScreen *screen)
{
	return [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
}

static bool display_uuid(NSScreen *screen, char *out, size_t size)
{
	CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(display_id(screen));
	CFStringRef text = uuid ? CFUUIDCreateString(NULL, uuid) : NULL;
	bool ok = false;

	if (text != NULL && size >= 42) {
		memcpy(out, "uuid:", 5);
		ok = CFStringGetCString(text, out + 5, size - 5, kCFStringEncodingUTF8);
		if (ok)
			for (size_t i = 5; out[i] != '\0'; i++)
				out[i] = (char) tolower((unsigned char) out[i]);
	}
	if (text) CFRelease(text);
	if (uuid) CFRelease(uuid);
	return ok;
}

bool displays_persistent_name(uint32_t display, char *out, size_t size)
{
	@autoreleasepool {
		if (display == 0) {
			snprintf(out, size, "primary");
			return true;
		}
		NSArray<NSScreen *> *screens = NSScreen.screens;
		return display <= screens.count && display_uuid(screens[display - 1], out, size);
	}
}

bool displays_resolve(const char *selection, bool fallback, uint32_t *display)
{
	@autoreleasepool {
		NSArray<NSScreen *> *screens = NSScreen.screens;

		*display = 0;
		if (strcmp(selection, "primary") == 0) return true;
		if (strncmp(selection, "uuid:", 5) == 0) {
			for (NSUInteger i = 0; i < screens.count; i++) {
				char uuid[48];

				if (display_uuid(screens[i], uuid, sizeof uuid) && strcmp(uuid, selection) == 0) {
					*display = (uint32_t) i + 1;
					return true;
				}
			}
		} else {
			unsigned long number = strtoul(selection, NULL, 10);

			if (number > 0 && number <= screens.count) {
				*display = (uint32_t) number;
				return true;
			}
		}
		fprintf(stderr, "u7: %s display %s is unavailable; %s\n", fallback ? "saved" : "requested",
			selection, fallback ? "using primary display" : "use --list-displays to see connected displays");
		return fallback;
	}
}

bool displays_list(void)
{
	@autoreleasepool {
		NSArray<NSScreen *> *screens = NSScreen.screens;

		if (screens.count == 0) {
			fprintf(stderr, "u7: no displays available\n");
			return false;
		}
		for (NSUInteger i = 0; i < screens.count; i++) {
			NSScreen *screen = screens[i];
			NSSize desktop = screen.frame.size, usable = screen.visibleFrame.size;
			char uuid[48] = "unavailable";

			display_uuid(screen, uuid, sizeof uuid);

			printf("%lu: %s%s; desktop %.0fx%.0f points; usable %.0fx%.0f points; "
				"backing scale %.2gx; ID %u\n", (unsigned long) i + 1,
				screen.localizedName.UTF8String, i == 0 ? " (primary)" : "",
				desktop.width, desktop.height, usable.width, usable.height,
				screen.backingScaleFactor, display_id(screen));
			printf("   Settings display value: %s\n", uuid);
		}
		return true;
	}
}

bool displays_window_frame(uint32_t display, uint32_t scale, MTY_Frame *frame)
{
	@autoreleasepool {
		NSArray<NSScreen *> *screens = NSScreen.screens;
		NSUInteger index = display > 0 ? display - 1 : 0;

		if (screens.count == 0) {
			fprintf(stderr, "u7: no displays available\n");
			return false;
		}
		if (index >= screens.count) {
			fprintf(stderr, "u7: display %u is unavailable; use --list-displays "
				"to see connected displays\n", display);
			return false;
		}
		NSScreen *screen = screens[index];
		NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
			NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
		/* Convert the usable outer-window bounds to content bounds, accounting for the
		 * title bar and borders using the same style as libmatoya's window. */
		NSRect content = [NSWindow contentRectForFrameRect:screen.visibleFrame styleMask:style];
		double maximum = floor(fmin(content.size.width / 320, content.size.height / 240));

		if (maximum < 1) {
			fprintf(stderr, "u7: display %lu has too little usable space for a 320x240 window\n",
				(unsigned long) index + 1);
			return false;
		}
		if (scale > maximum) {
			fprintf(stderr, "u7: --scale=%u reduced to %.0f to fit display %lu (%s)\n",
				scale, maximum, (unsigned long) index + 1, screen.localizedName.UTF8String);
			scale = (uint32_t) maximum;
		}
		memset(frame, 0, sizeof *frame);
		frame->size = (MTY_Size) {320 * scale, 240 * scale};
		/* libmatoya's macOS implementation takes content coordinates relative to the
		 * selected screen's origin, using AppKit's bottom-left coordinate system. */
		frame->x = (int32_t) floor(NSMidX(content) - frame->size.w / 2.0 - screen.frame.origin.x);
		frame->y = (int32_t) floor(NSMidY(content) - frame->size.h / 2.0 - screen.frame.origin.y);
		snprintf(frame->screen, sizeof frame->screen, "%u", display_id(screen));
		return true;
	}
}
