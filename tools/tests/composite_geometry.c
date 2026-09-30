#include <assert.h>
#include <stdio.h>
#include "videoout_geometry.h"
static struct videoout_geometry calc(int n,int d,int wide,int fill,int over,int ui)
{
    struct videoout_geometry g;
    assert(videoout_calc_geometry(640,480,n,d,720,480,
        (struct videoout_rect){0,0,720,480},wide,fill,over,ui,&g));
    return g;
}
int main(void)
{
    struct videoout_geometry g=calc(4,3,0,0,0,1);
    assert(g.destination.w==720 && g.destination.h==480);
    g=calc(4,3,1,0,0,1);
    assert(g.destination.w==540 && g.destination.h==480 && g.destination.x==90);
    g=calc(16,9,0,0,0,0);
    assert(g.destination.w==720 && g.destination.h==360 && g.destination.y==60);
    g=calc(16,9,1,0,0,0);
    assert(g.destination.w==720 && g.destination.h==480);
    g=calc(16,9,0,1,0,0);
    assert(g.crop.w==480 && g.crop.h==480 && g.crop.x==80);
    g=calc(4,3,1,1,0,0);
    assert(g.crop.w==640 && g.crop.h==360 && g.crop.y==60);
    for(int w=2;w<1000;w+=26) for(int h=2;h<800;h+=18)
    for(int wide=0;wide<2;wide++) for(int fit=0;fit<2;fit++)
    for(int over=0;over<4;over++) for(int ui=0;ui<2;ui++)
    {
        assert(videoout_calc_geometry(w,h,w,h,720,480,
            (struct videoout_rect){0,0,720,480},wide,fit,over,ui,&g));
        assert(g.crop.x>=0 && g.crop.y>=0 && g.crop.w>=2 && g.crop.h>=2);
        assert(g.crop.x+g.crop.w<=w && g.crop.y+g.crop.h<=h);
        assert(g.destination.x>=0 && g.destination.y>=0);
        assert(g.destination.x+g.destination.w<=720);
        assert(g.destination.y+g.destination.h<=480);
        assert(!((g.crop.x|g.crop.y|g.crop.w|g.crop.h|g.destination.x|
                  g.destination.y|g.destination.w|g.destination.h)&1));
        if(ui) assert(g.crop.w==w && g.crop.h==h);
        if(!ui && fit) assert(g.destination.w==720 && g.destination.h==480);
    }
    assert(!videoout_calc_geometry(0,240,4,3,720,480,
        (struct videoout_rect){0,0,720,480},0,0,0,1,&g));
    assert(!videoout_calc_geometry(320,240,4,0,720,480,
        (struct videoout_rect){0,0,720,480},0,0,0,1,&g));
    puts("PASS: TV geometry matrix, crop, insets, alignment, invalid inputs");
}
