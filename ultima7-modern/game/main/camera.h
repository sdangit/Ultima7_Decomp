#ifndef CAMERA_H
#define CAMERA_H

#include "mapview.h"

/* Needs objref declared first. */
struct Coord;
struct CellCoord;
struct ItemId;

/* What the world view follows: an item, or a fixed spot while locked. */
struct Camera {
	uint8_t waitForKey, drawOverlay, locked;
	objref target;
	Camera() { drawOverlay = 0; waitForKey = 0; }
	void setOverlay(uint8_t enabled);
	Coord getCenterX();
	Coord getCenterY();
	int16_t isOnScreen(Coord x, Coord y);
	void toWorldCoords(int16_t x, int16_t y, Coord *outX, Coord *outY);
	void moveTo(Coord x, Coord y);
	void moveToItem(Coord x, Coord y, ItemId item);
	void beginFrame();
	void render();
	void setTarget(objref item);
};

extern Camera gCamera;
extern WorldView MainWorldView;
extern int8_t FirstFramePending;
extern int8_t CopyingFrame;

void DrawCeilingMask(void);
void CopyFrameBuffer(void);
void FinishFrame(int8_t advance);
int16_t IsWorldPosOnScreen(Coord a, Coord b);
/* Paints the columns outside the centred 320 black, around a full-screen picture made for 320. */
void BlackenScreenSides(struct View *view);
/* Within the whole view, however wide; IsWorldPosOnScreen keeps the 320-pixel view's extent. */
int16_t IsWorldPosInView(Coord a, Coord b);
/* Far enough from the Avatar to move an NPC unseen: distance cells (the most of x, y and height
 * apart), as on the 320-pixel view; sideways, a wider view also needs as much past its edge. */
int16_t IsOutOfSight(Coord x1, Coord y1, int16_t z1, Coord x2, Coord y2, int16_t z2, int16_t distance);
void Camera_drawAt(Camera *self, CellCoord x, CellCoord y, uint8_t z);
void Camera_drawAt(Camera *self, Coord x, Coord y, ItemId item);
void DrawWorld(Camera *self);
void InitCamera(int16_t);
void InitChunkCache(void);
void InitMap(void);
void CenterOnAvatar(void);

#endif
