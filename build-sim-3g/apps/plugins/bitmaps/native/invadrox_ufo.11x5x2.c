#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim-3g/pluginbitmaps/invadrox_ufo.h"
const unsigned char invadrox_ufo[] = {
0xff, 0x57, 0xfc, 
0xf5, 0x55, 0x7c, 
0xdd, 0x75, 0xdc, 
0x55, 0x55, 0x54, 
0xf7, 0xff, 0x7c, 

};

const struct bitmap bm_invadrox_ufo = { 
    .width = BMPWIDTH_invadrox_ufo, 
    .height = BMPHEIGHT_invadrox_ufo, 
    .format = FORMAT_NATIVE, 
    .data = (unsigned char*)invadrox_ufo,
};
