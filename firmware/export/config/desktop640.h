/* Software-only Desktop Mode layout target. Never install on hardware. */
#include "desktop1080.h"
#undef MODEL_NUMBER
#undef MODEL_NAME
#undef LCD_WIDTH
#undef LCD_HEIGHT
#undef PLUGIN_BUFFER_SIZE
#define MODEL_NUMBER 211
#define MODEL_NAME "Desktop Mode (640x480 simulator)"
#define LCD_WIDTH 640
#define LCD_HEIGHT 480
#define PLUGIN_BUFFER_SIZE 0x800000

#ifndef SIMULATOR
#error desktop640 is a simulator-only target
#endif
