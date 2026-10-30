#include <stdint.h>
#include "dollpics.h"
#include "dollheads.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const struct { int16_t npc, picture; } Heads[] = {
	{ 2, DOLLPIC_SPARK_HEAD },
	{ 5, DOLLPIC_JAANA_HEAD },
	{ 7, DOLLPIC_SENTRI_HEAD },
	{ 8, DOLLPIC_JULIA_HEAD },
	{ 9, DOLLPIC_KATRINA_HEAD },
	{ 10, DOLLPIC_TSERAMED_HEAD },
};

int16_t BgCompanionHead(int16_t npc)
{
	for (uint16_t i = 0; i < COUNT(Heads); i++)
		if (Heads[i].npc == npc)
			return Heads[i].picture;
	return -1;
}
