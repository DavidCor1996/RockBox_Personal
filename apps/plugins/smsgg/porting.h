#ifndef SMSGG_PORTING_H
#define SMSGG_PORTING_H

#include <stddef.h>
#include <stdint.h>

#define IEXTFLASH_ATTR
#define DEXTFLASH_ATTR
#define IRAM_ATTR
#define DRAM_ATTR
#define MEM_SLOW 0
#define IS_LITTLE_ENDIAN 1

void *smsgg_malloc(size_t size);
void *smsgg_calloc(size_t nmemb, size_t size);
void smsgg_free(void *ptr);
void smsgg_abort(void);
int smsgg_log(const char *fmt, ...);
unsigned int smsgg_crc32_le(unsigned int crc, const unsigned char *buf,
                            unsigned int len);

#define rg_alloc(size, attr) smsgg_malloc(size)
#define rg_free(ptr) smsgg_free(ptr)
#define malloc(size) smsgg_malloc(size)
#define calloc(nmemb, size) smsgg_calloc(nmemb, size)
#define free(ptr) smsgg_free(ptr)
#define abort() smsgg_abort()
#define printf(...) smsgg_log(__VA_ARGS__)
#define crc32_le(crc, buf, len) smsgg_crc32_le((crc), (buf), (len))

#endif
