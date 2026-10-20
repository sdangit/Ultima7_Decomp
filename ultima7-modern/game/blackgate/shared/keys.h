#ifndef BG_SHARED_KEYS_H
#define BG_SHARED_KEYS_H

namespace BgShared {

/* The console keyboard, as the helpers read it with kbhit and getch. */

/* Handles waiting events; nonzero when a key is waiting. */
int16_t KeyHit(void);
/* Waits for a key: ASCII, or 0 and then the scan code on the next call. */
int16_t GetKey(void);

}

#endif
