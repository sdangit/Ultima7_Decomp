/* Smooth stereo uses a separate chip so classic FM keeps its timing. */
#define OPL_ENABLE_STEREOEXT 1
#define OPL3_Generate U7Opl3Ext_Generate
#define OPL3_GenerateResampled U7Opl3Ext_GenerateResampled
#define OPL3_Reset U7Opl3Ext_Reset
#define OPL3_WriteReg U7Opl3Ext_WriteReg
#define OPL3_WriteRegBuffered U7Opl3Ext_WriteRegBuffered
#define OPL3_GenerateStream U7Opl3Ext_GenerateStream
#define OPL3_Generate4Ch U7Opl3Ext_Generate4Ch
#define OPL3_Generate4ChResampled U7Opl3Ext_Generate4ChResampled
#define OPL3_Generate4ChStream U7Opl3Ext_Generate4ChStream
#include "opl3.c"
#include "opl3stereo.h"

static opl3_chip stereo_chip;

void opl3stereo_reset(uint32_t rate)
{
	OPL3_Reset(&stereo_chip, rate);
}

void opl3stereo_generate(int16_t frame[2])
{
	OPL3_Generate(&stereo_chip, frame);
}

void opl3stereo_write_buffered(uint16_t reg, uint8_t value)
{
	OPL3_WriteRegBuffered(&stereo_chip, reg, value);
}
