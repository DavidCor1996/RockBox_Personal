#ifndef RSC_ROCKBOX_PLATFORM_H
#define RSC_ROCKBOX_PLATFORM_H

#include "plugin.h"
#include <tlsf.h>

#define RSC_DATA_DIR ROCKBOX_DIR "/rocks/games/runescape_classic"
#define RSC_DEFAULT_NAME "David"
#define RSC_OFFLINE 1

void rsc_rb_init_alloc(void);
void *rsc_rb_malloc(size_t size);
void *rsc_rb_calloc(size_t nmemb, size_t size);
void *rsc_rb_realloc(void *ptr, size_t size);
void rsc_rb_free(void *ptr);

int rsc_rb_open(const char *path, int flags, ...);
int rsc_rb_close(int fd);
ssize_t rsc_rb_read(int fd, void *buf, size_t count);
ssize_t rsc_rb_write(int fd, const void *buf, size_t count);
off_t rsc_rb_lseek(int fd, off_t offset, int whence);
char *rsc_rb_strtok(char *str, const char *delim);
size_t rsc_rb_strcspn(const char *s, const char *reject);
double rsc_rb_atof(const char *s);
double rsc_rb_sin(double x);
double rsc_rb_cos(double x);
double rsc_rb_sqrt(double x);
double rsc_rb_pow(double base, double exp);
float rsc_rb_powf(float base, float exp);
float rsc_rb_floorf(float x);
double rsc_rb_ceil(double x);
double rsc_rb_fmin(double x, double y);
int rsc_rb_system(const char *cmd);
void rsc_haptic_menu_move(void);
void rsc_haptic_menu_select(void);
void rsc_haptic_action(void);
void rsc_haptic_error(void);
void rsc_haptic_skill(void);

#define malloc rsc_rb_malloc
#define calloc rsc_rb_calloc
#define realloc rsc_rb_realloc
#define free rsc_rb_free
#define open rsc_rb_open
#define close rsc_rb_close
#define read rsc_rb_read
#define write rsc_rb_write
#define lseek rsc_rb_lseek
#define snprintf rb->snprintf
#define vsnprintf rb->vsnprintf
#define strncmp rb->strncmp
#define strcasecmp rb->strcasecmp
#define strncasecmp rb->strncasecmp
#define rand rb->rand
#define srand rb->srand
#define strtok rsc_rb_strtok
#define strcspn rsc_rb_strcspn
#define atof rsc_rb_atof
#ifndef SIMULATOR
#define sin rsc_rb_sin
#define cos rsc_rb_cos
#define sqrt rsc_rb_sqrt
#define pow rsc_rb_pow
#define powf rsc_rb_powf
#define floorf rsc_rb_floorf
#define ceil rsc_rb_ceil
#define fmin rsc_rb_fmin
#endif
#define system rsc_rb_system

#endif
