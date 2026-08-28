

#ifndef __LOADER_H__
#define __LOADER_H__

void loader_init(const char *s);
bool cleanup(void);
bool sram_save(void);
bool sn_load(void);
bool sn_save(void);

#endif
