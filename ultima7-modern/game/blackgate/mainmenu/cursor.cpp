/* Black Gate MAINMENU.EXE modules U7POINT and CURSOR: the mouse pointer, drawn over the screen
 * and saved under, or handed to the host when it draws the pointer itself.
 */

#include "u7port.h"
#include "plat.h"
#include "view.h"
#include "lowlevel.h"
#include "vooalloc.h"
#include "oops.h"
#include "u7manage.h"
#include "mouse.h"
#include "preload.h"
#include "flex.h"
#include "u7point.h"
#include "cursor.h"

namespace BgMainMenu {

View *CursorTarget = 0;
int16_t ArrowCenterX = 0;
int16_t ArrowCenterY = 0;
int16_t CursorFrame = 0;
int32_t PointerShapes = 0;
int32_t CursorSaveBuffer = 0;
Rect CursorRect;
int16_t CursorDrawFlags = 0;
int16_t CursorSaveMode = 0;

/* Loads the pointer shapes from a Flex entry into Voodoo memory, with room to save what they cover. */
void LoadPointerShapes(void *self, char *flexName, int16_t entry, int16_t x, int16_t y)
{
	CursorTarget = 0;
	ArrowCenterX = x;
	ArrowCenterY = y;
	Flex flex;
	FlexEntry shapes;
	flex.open(flexName);
	flex.getEntry(entry, &shapes);
	int32_t size = shapes.size;
	PointerShapes = AllocateVoodooMemory(&VoodooXmsBlock, size);
	if (PointerShapes == 0)
		ReportOutOfVoodooMemory();
	flex.readEntryToVoodoo(&shapes, PointerShapes, 0);
	flex.close();
	CursorDrawFlags = 0x101;
	int16_t width = GetMaxFrameWidth(PointerShapes, CursorDrawFlags) + 1;
	int16_t height = GetMaxFrameHeight(PointerShapes, CursorDrawFlags) + 1;
	CursorSaveBuffer = AllocateVoodooMemory(&VoodooXmsBlock, width * height + 100);
	CursorSaveMode = 0x101;
	if (CursorSaveBuffer == 0)
		ReportOutOfVoodooMemory();
	CursorRect.x = 0;
	CursorRect.y = 0;
	CursorRect.x1 = width;
	CursorRect.y1 = height;
	if (plat_cursor_is_hardware())
		AllocateHostCursor(width, height);
}

void MoveCursorHook(int16_t events, int16_t buttons, int16_t x, int16_t y)
{
	if (CursorDrawn != 0) {
		if (CursorTracking != 0) {
			EraseCursor();
			DrawCursorAt(x >> 1, y);
		}
	}
}

void EraseCursor(void)
{
	if (plat_cursor_is_hardware()) {
		plat_cursor_show(0);
		return;
	}
	/* No target yet: DOS used a view at address 0, whose clip box was empty. */
	if (CursorTarget != 0)
		RestoreRect(CursorTarget, CursorSaveBuffer, &CursorRect, CursorSaveMode);
}

void DrawCursorAt(int16_t x, int16_t y)
{
	Rect r;

	if (plat_cursor_is_hardware()) {
		SetHostCursorFrame(PointerShapes, CursorFrame, CursorDrawFlags);
		plat_cursor_show(MousePresent);
	} else {
		GetFrameBounds(&r, x, y, PointerShapes, CursorFrame, CursorDrawFlags);
		CursorRect.x1 = r.x + (CursorRect.x1 - CursorRect.x);
		CursorRect.y1 = r.y + (CursorRect.y1 - CursorRect.y);
		CursorRect.x = r.x;
		CursorRect.y = r.y;
		if (CursorTarget != 0) {
			SaveRect(CursorTarget, CursorSaveBuffer, &CursorRect, CursorSaveMode);
			if (MousePresent)
				DrawFrame(CursorTarget, x, y, PointerShapes, CursorFrame, CursorDrawFlags);
		}
	}
	CursorX = x;
	CursorY = y;
}

void InstallCursorHook(MouseHandler *handler)
{
	handler->attach(MoveCursorHook, MOUSE_MOVED);
}

void ShowCursor(void)
{
	uint8_t saved;

	saved = CursorTracking;
	CursorTracking = 0;
	if (CursorDrawn != 0)
		EraseCursor();
	DrawCursorAt(CursorX, CursorY);
	CursorDrawn = 1;
	CursorTracking = saved;
}

void HideCursor(void)
{
	uint8_t saved;

	saved = CursorTracking;
	CursorTracking = 0;
	if (CursorDrawn != 0)
		EraseCursor();
	CursorDrawn = 0;
	CursorTracking = saved;
}

/* Moves the cursor to another view, taking it off the old one first. */
void SetCursorTarget(View *buffer)
{
	uint8_t drawn;

	CursorTracking = 0;
	if ((drawn = CursorDrawn) != 0)
		HideCursor();
	CursorTarget = buffer;
	if (drawn)
		ShowCursor();
	CursorTracking = 1;
}

void DrawCursorInto(View *buffer)
{
	View *old;

	old = CursorTarget;
	CursorTarget = buffer;
	DrawCursorAt(GetMouseX() >> 1, GetMouseY());
	CursorTarget = old;
}

void EraseCursorFrom(View *buffer)
{
	View *old;

	old = CursorTarget;
	CursorTarget = buffer;
	EraseCursor();
	CursorTarget = old;
}

}

extern "C" void ResetBgMainMenuCursorGlobals(void)
{
	BgMainMenu::CursorTarget = 0;
	BgMainMenu::ArrowCenterX = 0;
	BgMainMenu::ArrowCenterY = 0;
	BgMainMenu::CursorFrame = 0;
	BgMainMenu::PointerShapes = 0;
	BgMainMenu::CursorSaveBuffer = 0;
	memset(&BgMainMenu::CursorRect, 0, sizeof BgMainMenu::CursorRect);
	BgMainMenu::CursorDrawFlags = 0;
	BgMainMenu::CursorSaveMode = 0;
}
