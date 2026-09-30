/* Native LCD sprite cache. Rasterize each pose once, then copy opaque spans.
 * Storage is the otherwise unused plugin buffer, never the playback heap. */
class SkullBlitCache
{
    enum { COUNT=256 };
    struct Entry {
        int id, key[12], w, h, left, top;
        unsigned int offset, bytes, used;
        fb_data *rgb;
        unsigned char *alpha;
        unsigned short *spans;
    } entries[COUNT];
    unsigned char *arena;
    unsigned int capacity, cursor, serial;
public:
    unsigned int hits, misses, pixels;
    SkullBlitCache() : arena(NULL),capacity(0),cursor(0),serial(0),hits(0),misses(0),pixels(0)
    { rb->memset(entries,0,sizeof(entries)); }
    void init(unsigned char *data, unsigned int size)
    {
        arena=data; capacity=size; cursor=serial=hits=misses=pixels=0;
        rb->memset(entries,0,sizeof(entries));
    }
    bool draw(int id, const SkullShape &shape, float ox,float oy,
              float ax,float ay,float bx,float by,
              const gameswf::cxform &color,fb_data *framebuf,
              unsigned char *mask,bool masked,bool submitting)
    {
        if(!arena || !framebuf) return false;
        int key[12];
        key[0]=(int)(ax*65536);key[1]=(int)(ay*65536);
        key[2]=(int)(bx*65536);key[3]=(int)(by*65536);
        for(int k=0;k<4;k++) {
            key[4+k*2]=(int)(color.m_[k][0]*256);
            key[5+k*2]=(int)color.m_[k][1];
        }
        Entry *entry=NULL;
        for(int i=0;i<COUNT;i++)
            if(entries[i].bytes && entries[i].id==id &&
               !rb->memcmp(entries[i].key,key,sizeof(key))) {entry=&entries[i];break;}
        if(entry) hits++;
        else {
            float x1=ax*shape.width,x2=bx*shape.height,x3=x1+x2;
            float y1=ay*shape.width,y2=by*shape.height,y3=y1+y2;
            int left=(int)floorf(MIN(0,MIN(x1,MIN(x2,x3))))-1;
            int top=(int)floorf(MIN(0,MIN(y1,MIN(y2,y3))))-1;
            int w=(int)ceilf(MAX(0,MAX(x1,MAX(x2,x3))))+1-left;
            int h=(int)ceilf(MAX(0,MAX(y1,MAX(y2,y3))))+1-top;
            if(w<=0 || h<=0 || w>4096 || h>1024) return false;
            unsigned int count=w*h;
            unsigned int bytes=(count*(sizeof(fb_data)+1)+h*6+3)&~3u;
            if(bytes>capacity) return false;
            if(cursor+bytes>capacity)cursor=0;
            unsigned int oldest=~0u;
            for(int i=0;i<COUNT;i++) {
                Entry &e=entries[i];
                if(e.bytes && e.offset<cursor+bytes && cursor<e.offset+e.bytes)e.bytes=0;
                if(!e.bytes || e.used<oldest) {entry=&e;oldest=e.bytes?e.used:0;}
            }
            entry->id=id;entry->offset=cursor;entry->bytes=bytes;
            rb->memcpy(entry->key,key,sizeof(key));
            entry->w=w;entry->h=h;entry->left=left;entry->top=top;
            entry->rgb=(fb_data *)(arena+cursor);
            entry->spans=(unsigned short *)(entry->rgb+count);
            entry->alpha=(unsigned char *)(entry->spans+h*3);
            cursor+=bytes;misses++;
            float det=ax*by-ay*bx;
            if(fabsf(det)<0.000001f) {entry->bytes=0;return true;}
            int du=(int)(by/det*65536),dv=(int)(-ay/det*65536);
            int eu=(int)(-bx/det*65536),ev=(int)(ax/det*65536);
            int row_u=(int)(((left+0.5f)*by-(top+0.5f)*bx)/det*65536)-32768;
            int row_v=(int)(((top+0.5f)*ax-(left+0.5f)*ay)/det*65536)-32768;
            for(int y=0;y<h;y++,row_u+=eu,row_v+=ev) {
                int u=row_u,v=row_v,first=w,last=0;bool opaque=true;
                for(int x=0;x<w;x++,u+=du,v+=dv) {
                    int ix=u>>16,iy=v>>16,fx=(u&65535)>>8,fy=(v&65535)>>8;
                    unsigned int sa=0,sr=0,sg=0,sb=0;
                    for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
                        int xx=ix+i,yy=iy+j;
                        if(xx<0||yy<0||xx>=shape.width||yy>=shape.height)continue;
                        const unsigned char *p=shape.pixels+(yy*shape.width+xx)*4;
                        unsigned int a=p[3]*(i?fx:256-fx)*(j?fy:256-fy);
                        sa+=a;sr+=p[0]*a/256;sg+=p[1]*a/256;sb+=p[2]*a/256;
                    }
                    int c[4]={0,0,0,0};
                    if(sa>=65536) {
                        c[0]=sr*256/sa;c[1]=sg*256/sa;c[2]=sb*256/sa;c[3]=sa>>16;
                        for(int k=0;k<4;k++)c[k]=MAX(0,MIN(255,((c[k]*key[4+k*2])>>8)+key[5+k*2]));
                    }
                    entry->rgb[y*w+x]=LCD_RGBPACK(c[0],c[1],c[2]);
                    entry->alpha[y*w+x]=c[3];
                    if(c[3]) {first=MIN(first,x);last=x+1;}
                }
                for(int x=first;x<last;x++)if(entry->alpha[y*w+x]!=255) {opaque=false;break;}
                entry->spans[y*3]=first;entry->spans[y*3+1]=last;entry->spans[y*3+2]=opaque;
            }
        }
        entry->used=++serial;
        int dest_x=(int)floorf(ox+0.5f)+entry->left;
        int dest_y=(int)floorf(oy+0.5f)+entry->top;
        int first_y=MAX(0,-dest_y),last_y=MIN(entry->h,LCD_HEIGHT-dest_y);
        for(int y=first_y;y<last_y;y++) {
            int first=MAX((int)entry->spans[y*3],MAX(0,-dest_x));
            int last=MIN((int)entry->spans[y*3+1],LCD_WIDTH-dest_x);
            if(last<=first)continue;
            fb_data *dst=framebuf+(dest_y+y)*LCD_WIDTH+dest_x+first;
            const fb_data *src=entry->rgb+y*entry->w+first;
            if(entry->spans[y*3+2] && !masked && !submitting)rb->memcpy(dst,src,(last-first)*sizeof(fb_data));
            else {
                const unsigned char *alpha=entry->alpha+y*entry->w+first;
                for(int x=first;x<last;x++,dst++,src++,alpha++) {
                    int a=*alpha;
                    int index=(dest_y+y)*LCD_WIDTH+dest_x+x;
                    if(submitting) {if(a && mask)mask[index]=1;continue;}
                    if(masked && (!mask || !mask[index]))continue;
                    if(a==255)*dst=*src;
                    else if(a) {
                        int inv=255-a;
                        int r=(RGB_UNPACK_RED(*src)*a+RGB_UNPACK_RED(*dst)*inv+127)/255;
                        int g=(RGB_UNPACK_GREEN(*src)*a+RGB_UNPACK_GREEN(*dst)*inv+127)/255;
                        int b=(RGB_UNPACK_BLUE(*src)*a+RGB_UNPACK_BLUE(*dst)*inv+127)/255;
                        *dst=LCD_RGBPACK(r,g,b);
                    }
                }
            }
            pixels+=last-first;
        }
        return true;
    }
};
