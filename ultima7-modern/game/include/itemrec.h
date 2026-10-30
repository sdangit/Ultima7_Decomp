#ifndef ITEMREC_H
#define ITEMREC_H

#include "typefram.h"
#include "objref.h"
#include "iteminfo.h"
#include "gametype.h"

/* Where an item lies: a cell in its region on the map, or its container. */
union ItemPosition {
	struct { uint8_t x, y; } world;
	int16_t parent;
};

/* The 8-byte record in the item buffer. */
struct ItemRecord {
	int16_t next;
	ItemPosition position;
	uint16_t typeFrame;
	union {
		int16_t extra;
		struct { uint8_t z, quality; } direct;
	} data;
};

/* A record's first six bytes, copied to draw an item without changing it. */
struct ItemSnapshot {
	int16_t next;
	ItemPosition position;
	TypeFrame typeFrame;
	ItemSnapshot() {}
};

/* The item whose record this is, for a record in the item buffer. */
inline objref RecordItem(const ItemRecord *rec) { return (int16_t)((const uint8_t *)rec - ItemBufferBase); }

/* Which game's files each item record comes from, one byte per record. */
extern uint8_t ItemGameTypes[0x10000 / sizeof(ItemRecord)];

inline enum GameType Item_gameType(objref item)
{
	return (enum GameType)ItemGameTypes[(uint16_t)item.off / sizeof(ItemRecord)];
}

inline void Item_setGameType(objref item, enum GameType game)
{
	ItemGameTypes[(uint16_t)item.off / sizeof(ItemRecord)] = (uint8_t)game;
}

/* An item's shape, frame and flip flag. Only these and Item_setFrame touch them in the record;
 * the shape's game goes to the side table. */
inline TypeFrame Item_typeFrame(objref item)
{
	TypeFrame value(item.ptr()->typeFrame);
	enum GameType game = Item_gameType(item);

	if (game != PlayedGame())
		value.high = GameShape(game, 0) >> 10;
	return value;
}

inline void Item_setTypeFrame(objref item, TypeFrame value)
{
	item.ptr()->typeFrame = value.bits;
	Item_setGameType(item, ShapeGame(value.type()));
}
inline uint16_t Item_shape(objref item) { return Item_typeFrame(item).type(); }
inline uint16_t Item_frame(objref item) { return Item_typeFrame(item).frame(); }
inline uint8_t Item_flipped(objref item) { return Item_typeFrame(item).flipped(); }

inline void Item_setShape(objref item, uint16_t shape)
{
	TypeFrame value = Item_typeFrame(item);
	value.setType(shape);
	Item_setTypeFrame(item, value);
}

/* Takes the flip flag as frame bit 5. */
inline void Item_setFlipFrame(objref item, uint16_t frame)
{
	TypeFrame value = Item_typeFrame(item);
	value.setFlipFrame(frame);
	Item_setTypeFrame(item, value);
}

/* The next item in the same list. */
inline objref objref::next() { return ptr()->next; }
inline uint16_t objref::type() { return Item_shape(*this); }
inline uint16_t objref::frame() { return Item_frame(*this); }
inline uint16_t objref::z() { return GetItemZAndStuff(this).z(); }

/* An item's type number. */
inline uint16_t GetItemType(objref item) { return item.type(); }

/* Where an item lies: 0 to 3 a map region, then off the map, deleted, contained or equipped. */
inline uint8_t GetItemKind(objref *item)
{
	ItemInfo info = GetItemZAndStuff(item);
	return (uint8_t)((uint16_t)info.flags & 7);
}
inline uint8_t IsOnMap(objref *item)
{
	ItemInfo info = GetItemZAndStuff(item);
	return info.kind() <= 3;
}
inline uint8_t IsContained(objref *item)
{
	ItemInfo info = GetItemZAndStuff(item);
	return (uint8_t)((uint16_t)info.flags & 7) >= 6;
}
inline uint8_t IsTemporary(objref *item) { return GetItemZAndStuff(item).flags & 8; }

#endif
