#ifndef ROCKBOX_CPS1_CACHE_H
#define ROCKBOX_CPS1_CACHE_H
extern int bBurnUseRomCache;
unsigned int BurnCacheBlockSize(int blockid);
int BurnCacheRead(unsigned char *buffer, int blockid);
#endif
