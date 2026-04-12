#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim/pluginbitmaps/superdom_boarditems.h"
const unsigned char superdom_boarditems[] = {
0xf0, 0x3f, 0xf5, 0x31, 0xf0, 0x3f, 0xf5, 0x31, 
0xac, 0x7e, 0xac, 0x00, 0xac, 0x7e, 0xac, 0x00, 
0xff, 0x25, 0x45, 0xf2, 0xff, 0x25, 0x45, 0xf2, 

};

const struct bitmap bm_superdom_boarditems = { 
    .width = BMPWIDTH_superdom_boarditems, 
    .height = BMPHEIGHT_superdom_boarditems, 
    .data = (unsigned char*)superdom_boarditems,
};
