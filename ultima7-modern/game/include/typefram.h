#ifndef TYPEFRAM_H
#define TYPEFRAM_H

/* A type in the low ten bits, a frame in the next five, and a flip flag in the top bit. A type
 * from another game has more bits; those above ten are kept in high, outside the word. */
struct TypeFrame {
	uint16_t bits;
	uint16_t high;
	TypeFrame() { high = 0; }
	TypeFrame(uint16_t n) { bits = n; high = 0; }
	TypeFrame(uint16_t type, uint16_t frame) { bits = (type & 0x3ff) | ((frame << 10) & 0x7c00); high = type >> 10; }
	uint16_t type() { return (bits & 0x3ff) | (high << 10); }
	uint16_t frame() { return (bits & 0x7c00) >> 10; }
	uint8_t flipped() { return (bits & 0x8000) == 0x8000; }
	/* The frame with the flip flag as bit 5. */
	uint16_t flipFrame() { return bits >> 10; }
	void setType(uint16_t type) { bits = (type & 0x3ff) | (bits & 0xfc00); high = type >> 10; }
	/* Keeps the flip flag. */
	void setFrame(uint16_t frame) { bits = (bits & 0x83ff) | ((frame << 10) & 0x7c00); }
	/* Takes the flip flag as frame bit 5. */
	void setFlipFrame(uint16_t frame) { bits = (bits & 0x3ff) | ((frame << 10) & 0xfc00); }
	/* Same type, frame and flip. */
	uint8_t same(TypeFrame other) { return bits == other.bits && high == other.high; }
};

#endif
