#ifndef WEIGHT_H
#define WEIGHT_H

#include "gametype.h"

struct WeightVolume;

struct objref;

int16_t GetItemTypeWeight(int16_t type);
int16_t Item_getWeight(objref item);
int16_t GetItemTypeBulk(int16_t type);
void LoadWgtVol(char *name);
void LoadOtherWgtVol(enum GameType game);
int16_t DetermineWeightOfContents(objref item);
int16_t DetermineBulkOfContents(objref item);

extern WeightVolume *TypeWeightVolumes;

#endif
