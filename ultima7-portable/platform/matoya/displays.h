/* macOS display selection shared by both portable games. */
#ifndef U7_DISPLAYS_H
#define U7_DISPLAYS_H

#include <stdbool.h>
#include <stdint.h>
#include <matoya.h>

bool displays_list(void);
/* Saved selections may fall back to primary when disconnected; explicit CLI requests fail. */
bool displays_resolve(const char *selection, bool fallback, uint32_t *display);
bool displays_persistent_name(uint32_t display, char *out, size_t size);
/* Display numbers are one-based; 0 selects the primary display. */
bool displays_window_frame(uint32_t display, uint32_t scale, MTY_Frame *frame);

#endif
