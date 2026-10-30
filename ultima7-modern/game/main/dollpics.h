/* Made by tools/dollart.py from assets/paperdoll; do not edit. */
#ifndef DOLLPICS_H
#define DOLLPICS_H

#include <stdint.h>

#define DOLLPIC_KIDNEY_BELT 170
#define DOLLPIC_SPARK_HEAD 171
#define DOLLPIC_JAANA_HEAD 172
#define DOLLPIC_SENTRI_HEAD 173
#define DOLLPIC_JULIA_HEAD 174
#define DOLLPIC_KATRINA_HEAD 175
#define DOLLPIC_TSERAMED_HEAD 176
#define DOLLPIC_CLEAVER 177
#define DOLLPIC_MUSKET 178
#define DOLLPIC_STARBURST 179
#define DOLLPIC_FLAMING_OIL 180
#define DOLLPIC_HAWK 181
#define DOLLPIC_GREAT_DAGGER 182
#define DOLLPIC_KNIFE 183
#define DOLLPIC_MAIN_GAUCHE 184
#define DOLLPIC_CUSTOM_SWORD 185
#define DOLLPIC_FELLOWSHIP_STAFF 186
#define DOLLPIC_TRIPLE_CROSSBOW 187
#define DOLLPIC_DEATH_SCYTHE 188
#define DOLLPIC_HOE_OF_DESTRUCTION 189
#define DOLLPIC_CLOAK_GREY 190
#define DOLLPIC_CLOAK_GREEN 191
#define DOLLPIC_AVATAR_COSTUME 192
#define DOLLPIC_TOP_GREEN 193
#define DOLLPIC_TOP_ORANGE 194
#define DOLLPIC_TOP_BLUE 195
#define DOLLPIC_PANTS 196
#define DOLLPIC_SHOES 197
#define DOLLPIC_HOOD 198
#define DOLLPIC_CADDELLITE_HELM 199
#define DOLLPIC_GORGET 11
#define DOLLPIC_MAGIC_GORGET 27
#define DOLLPIC_LEATHER_COLLAR 40
#define DOLLPIC_LUTE 55

/* Our picture numbered so, its size in bytes, or 0. */
const uint8_t *OwnDollPicture(int16_t picture, int32_t *size);

/* A picture made at run time from a Serpent Isle one: the frames whose bits are set in
 * emptyFrames left empty, and each colour run first..last moved to start at to. */
struct DerivedDollPicture {
	int16_t picture, source;
	uint32_t emptyFrames;
	uint8_t moveCount;
	struct { uint8_t first, last, to; } moves[4];
};

/* How the picture numbered so is made, or 0. */
const struct DerivedDollPicture *FindDerivedDollPicture(int16_t picture);

#endif
