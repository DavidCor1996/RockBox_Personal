#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim-3g/pluginbitmaps/invadrox_alien_explode.h"
const unsigned char invadrox_alien_explode[] = {
0xcf, 0x3c, 0x30, 
0xf3, 0x33, 0xf0, 
0x0f, 0xff, 0x00, 
0xfc, 0xcc, 0xf0, 
0xc3, 0xcf, 0x30, 

};

const struct bitmap bm_invadrox_alien_explode = { 
    .width = BMPWIDTH_invadrox_alien_explode, 
    .height = BMPHEIGHT_invadrox_alien_explode, 
    .format = FORMAT_NATIVE, 
    .data = (unsigned char*)invadrox_alien_explode,
};
