/* macOS display selection shared by both portable games. */
#ifndef U7_DISPLAYS_H
#define U7_DISPLAYS_H

#include <stdbool.h>
#include <stdint.h>
#include <matoya.h>

bool displays_list(void);
/* Display numbers are one-based; 0 selects the primary display. */
bool displays_window_frame(uint32_t display, uint32_t scale, MTY_Frame *frame);

#endif
