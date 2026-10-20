#ifndef U7_OPL3_STEREO_H
#define U7_OPL3_STEREO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opl3stereo_reset(uint32_t rate);
void opl3stereo_generate(int16_t frame[2]);
void opl3stereo_write_buffered(uint16_t reg, uint8_t value);

#ifdef __cplusplus
}
#endif
#endif
