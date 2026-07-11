#ifndef _SHARED_H_
#define _SHARED_H_

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;

typedef signed char int8;
typedef signed short int int16;
typedef signed int int32;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <porting.h>

#ifndef PATH_MAX
#ifdef  MAX_PATH
#define PATH_MAX    MAX_PATH
#else
#define PATH_MAX    1024
#endif
#endif

#undef PATH_MAX
#define PATH_MAX 255

#include "cpu/z80.h"
#include "sms.h"
#include "pio.h"
#include "memz80.h"
#include "vdp.h"
#include "render.h"
#include "tms.h"
#include "sound/sn76489.h"
#include "sound/emu2413.h"
#include "sound/ym2413.h"
#include "sound/fmintf.h"
#include "sound/sms_sound.h"
#include "system.h"
#include "loadrom.h"
// #include "config.h"
#include "state.h"

#endif /* _SHARED_H_ */
