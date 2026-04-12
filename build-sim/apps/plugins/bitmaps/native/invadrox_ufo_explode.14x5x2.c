#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim/pluginbitmaps/invadrox_ufo_explode.h"
const unsigned char invadrox_ufo_explode[] = {
0xdf, 0xdf, 0xfd, 0xf0, 
0xff, 0xd5, 0x77, 0xf0, 
0xfd, 0x77, 0x5d, 0xd0, 
0x7d, 0xd7, 0xf7, 0xf0, 
0xdf, 0xff, 0x7f, 0xf0, 

};

const struct bitmap bm_invadrox_ufo_explode = { 
    .width = BMPWIDTH_invadrox_ufo_explode, 
    .height = BMPHEIGHT_invadrox_ufo_explode, 
    .format = FORMAT_NATIVE, 
    .data = (unsigned char*)invadrox_ufo_explode,
};
