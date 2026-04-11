#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim-nano2g/pluginbitmaps/bubbles_bubble.h"
const unsigned char bubbles_bubble[] = {
0xf0, 0x0c, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x0c, 0xf0, 
0x00, 0x03, 0x04, 0x04, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x03, 0x00, 

};

const struct bitmap bm_bubbles_bubble = { 
    .width = BMPWIDTH_bubbles_bubble, 
    .height = BMPHEIGHT_bubbles_bubble, 
    .data = (unsigned char*)bubbles_bubble,
};
