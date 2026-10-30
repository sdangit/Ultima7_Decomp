#ifndef TEXT_H
#define TEXT_H

char *GetGameText(uint8_t section, uint16_t n);
void InitTextCache(void);
/* Item names (text sections 0 and 2) from a game's own file until set back to -1. */
void SetItemNameGame(int8_t game);

#ifdef __cplusplus
extern "C" {
#endif
void ReportNoCanDo(uint8_t why);
#ifdef __cplusplus
}
#endif

#endif
