/* The start menu's moongates: a tall doorway with one swirling arm, banded as Black
 * Gate's own gate art is, its depth showing along the top and right. Frames 0-3 rise out of
 * the ground, 4-11 turn the swirl once.
 */
#ifndef STARTMENU_GATE_H
#define STARTMENU_GATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GATE_FACE_WIDTH 38
#define GATE_FACE_HEIGHT 50
#define GATE_DEPTH 2
#define GATE_WIDTH (GATE_FACE_WIDTH + GATE_DEPTH)
#define GATE_HEIGHT (GATE_FACE_HEIGHT + GATE_DEPTH)
#define GATE_RISE_FRAMES 4
#define GATE_FRAMES 12

/* Pixel values: shades 0 (darkest) to 7, the side, and nothing. */
#define GATE_SHADES 8
#define GATE_SIDE 8
#define GATE_EMPTY 255

void GateDrawFrame(uint8_t pixels[GATE_HEIGHT][GATE_WIDTH], int16_t frame);

#ifdef __cplusplus
}
#endif

#endif
