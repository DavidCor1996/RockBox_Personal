/* Raster caches of the original SWF vector shapes, exported at LCD scale.
 * The authored timeline, matrices, color transforms and collision shapes remain
 * in the SWF. No redrawn substitutes or disk reads occur during painting. */
struct SkullShape {const unsigned char *pixels; int width,height,left,top,source_width,source_height;};
static SkullShape skull_shapes[1100];
static unsigned char *skull_shape_data;
static unsigned int skull_u32(const unsigned char *p)
{return p[0]|(p[1]<<8)|(p[2]<<16)|((unsigned int)p[3]<<24);}
static bool load_skull_shapes()
{
    if(!g_skullkid || skull_shape_data) return true;
    int fd=rb->open(FLASH_DIR "/skullkid/art.shapes",O_RDONLY);
    if(fd<0) return false;
    long size=rb->filesize(fd);
    if(size<8 || size>12*1024*1024 || (size_t)size+18*1024*1024>plugin_cxx_available()) {rb->close(fd);return false;}
    unsigned char *data=new unsigned char[size];
    if(!data) {rb->close(fd);return false;}
    long got=rb->read(fd,data,size);rb->close(fd);
    unsigned int count=skull_u32(data+4);
    if(got!=size || rb->memcmp(data,"NGS2",4) || count>1100 || 8+count*24>(unsigned int)size) {delete[]data;return false;}
    unsigned int base=8+count*24;
    for(unsigned int i=0;i<count;i++) {
        const unsigned char *p=data+8+i*24;
        unsigned int id=skull_u32(p),width=p[4]|p[5]<<8,height=p[6]|p[7]<<8,offset=skull_u32(p+16);
        if(id>=1100 || !width || !height || !(p[20]|p[21]<<8) || !(p[22]|p[23]<<8) || offset>(unsigned int)size-base ||
            (unsigned long long)width*height*4>(unsigned int)size-base-offset) {delete[]data;return false;}
    }
    for(unsigned int i=0;i<count;i++) {
        const unsigned char *p=data+8+i*24; SkullShape &shape=skull_shapes[skull_u32(p)];
        shape.width=p[4]|p[5]<<8;shape.height=p[6]|p[7]<<8;
        shape.source_width=p[20]|p[21]<<8;shape.source_height=p[22]|p[23]<<8;
        shape.left=(int)skull_u32(p+8);shape.top=(int)skull_u32(p+12);
        shape.pixels=data+base+skull_u32(p+16);
    }
    skull_shape_data=data;
    flash_logf("skull original artwork shapes=%u bytes=%ld",count,size);
    return true;
}
static void close_skull_shapes()
{delete[]skull_shape_data;skull_shape_data=NULL;rb->memset(skull_shapes,0,sizeof(skull_shapes));}
