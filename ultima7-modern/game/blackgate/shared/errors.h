#ifndef BG_SHARED_ERRORS_H
#define BG_SHARED_ERRORS_H

/* U7's fatal hook chain, which U7 code reporting an error runs too. */
#include "../main/mem/errors.h"

namespace BgShared {

/* Formats the message, runs the fatal hook, prints it and ends the program with code 1. */
void FatalError(char *fmt, ...);

}

#endif
