#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim/pluginbitmaps/invadrox_fire.h"
const unsigned char invadrox_fire[] = {
0x29, 0x12, 0x1f, 0x1e, 0x10, 0x25, 

};

const struct bitmap bm_invadrox_fire = { 
    .width = BMPWIDTH_invadrox_fire, 
    .height = BMPHEIGHT_invadrox_fire, 
    .data = (unsigned char*)invadrox_fire,
};
