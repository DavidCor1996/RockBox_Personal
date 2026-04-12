#include "lcd.h"
#include "/home/david/Documents/RockBox_Personal-master/build-sim/pluginbitmaps/brickmania_ball.h"
const unsigned char brickmania_ball[] = {
0x3f, 0x00, 
0xff, 0xc0, 
0xff, 0xc0, 
0xff, 0xc0, 
0x3f, 0x00, 

};

const struct bitmap bm_brickmania_ball = { 
    .width = BMPWIDTH_brickmania_ball, 
    .height = BMPHEIGHT_brickmania_ball, 
    .format = FORMAT_NATIVE, 
    .data = (unsigned char*)brickmania_ball,
};
