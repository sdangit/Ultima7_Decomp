#ifndef STARTITEMS_H
#define STARTITEMS_H

extern char *const StartItemsFileName;
void SaveStartItems(char *dir);
void LoadStartItems(char *dir);
/* Gives the items not given yet whose place is at hand: the Avatar's pack, or a container in a
 * loaded region. */
void PlaceStartItems(void);

#endif
