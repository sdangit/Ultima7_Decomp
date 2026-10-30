#include <math.h>
#include <string.h>
#include "gate.h"

/* Inside the face, rows top to top + height - 1. */
static int16_t InFace(int16_t x, int16_t y, int16_t top, int16_t height)
{
	return x >= 0 && x < GATE_FACE_WIDTH && y >= top && y < top + height;
}

/* The face: mid red, darker toward the rim and at the eye, and one lighter arm winding out
 * from the eye; turn moves the arm round (0 to 1 is a whole turn). */
static uint8_t FaceShade(int16_t x, int16_t y, int16_t top, int16_t height, float turn)
{
	float dx = (x - (GATE_FACE_WIDTH - 1) / 2.0f) / (GATE_FACE_WIDTH / 2.0f);
	float dy = (y - (top + height * 0.55f)) / (height / 2.0f);
	float r = sqrtf(dx * dx + dy * dy);
	float angle = atan2f(dy, dx) / 6.2831853f;
	float along = angle + r * 1.15f - turn;
	float crest, arm, light;

	along -= floorf(along);
	crest = 1.0f - fabsf(along - 0.5f) / 0.32f;
	arm = crest > 0 ? crest : 0;
	light = 0.55f - 0.45f * (r < 1 ? r : 1) - (r < 0.18f ? 0.15f : 0)
		+ 0.75f * arm * (r * 3 < 1 ? r * 3 : 1);
	light *= GATE_SHADES;
	return (uint8_t) (light < 0 ? 0 : light > GATE_SHADES - 1 ? GATE_SHADES - 1 : light);
}

void GateDrawFrame(uint8_t pixels[GATE_HEIGHT][GATE_WIDTH], int16_t frame)
{
	int16_t height = frame < GATE_RISE_FRAMES
		? (int16_t) (GATE_FACE_HEIGHT * (frame + 1) / (GATE_RISE_FRAMES + 1)) : GATE_FACE_HEIGHT;
	float turn = frame < GATE_RISE_FRAMES ? 0 : (float) (frame - GATE_RISE_FRAMES)
		/ (GATE_FRAMES - GATE_RISE_FRAMES);
	int16_t top = (int16_t) (GATE_HEIGHT - height);
	int16_t x, y, d;

	memset(pixels, GATE_EMPTY, GATE_HEIGHT * GATE_WIDTH);
	for (y = 0; y < GATE_HEIGHT; ++y) {
		for (x = 0; x < GATE_WIDTH; ++x) {
			if (InFace(x, y, top, height)) {
				pixels[y][x] = FaceShade(x, y, top, height, turn);
				continue;
			}
			/* the side: the face moved back up and to the right */
			for (d = 1; d <= GATE_DEPTH; ++d) {
				if (InFace((int16_t) (x - d), (int16_t) (y + d), top, height))
					pixels[y][x] = GATE_SIDE;
			}
		}
	}
}
