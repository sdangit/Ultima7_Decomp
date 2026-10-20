#ifndef DOLLART_H
#define DOLLART_H

#include "npcref.h"

/* Which paperdoll pictures each game's items and people use. The pictures are Serpent Isle's;
 * Black Gate's items take the look of a Serpent Isle item or one of our own pictures. */

/* FindDollPicture results */
#define DOLL_NO_PICTURE    0
#define DOLL_PICTURE       1
#define DOLL_OTHER_FRAME   2   /* the shape has pictures, but not for this frame */

/* The Serpent Isle item whose picture an item uses, -1 for none, and its frame. */
int16_t DollType(objref item);
uint16_t DollFrame(objref item);
uint8_t FindDollPicture(int16_t type, uint16_t frame, int16_t *picture);
/* As FindDollPicture, for the item, taking Black Gate's own pictures first. */
uint8_t FindItemPicture(objref item, uint8_t slot, int16_t *picture);
/* For our pictures holding a female and a male frame per item frame: the item's female frame,
 * else -1. */
int16_t DollPairFrame(objref item);

/* Makes picture how from Serpent Isle's picture in (size bytes) into out, which has room for
 * size plus 10 bytes a frame; returns the new size. */
struct DerivedDollPicture;
int32_t MakeDerivedDollPicture(const DerivedDollPicture *how, const uint8_t *in, int32_t size, uint8_t *out);

/* The body and head: Black Gate's Avatar, Iolo, Shamino and Dupre have Serpent Isle's; others
 * in Black Gate get a plain body and no face. */
int16_t DollNpcType(NPCRef npc);
int16_t DollNpcNumber(NPCRef npc);
int16_t DollOwnHead(NPCRef npc);
uint8_t DollHasHead(NPCRef npc);
uint8_t DollSkinColor(objref *ref);
uint8_t DollPetraFlag(objref *ref);
/* A Black Gate party member's head picture for the combat status window, or -1. */
int16_t DollFace(NPCRef npc);

#endif
