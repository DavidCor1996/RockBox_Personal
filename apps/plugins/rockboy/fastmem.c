

#include "rockmacros.h"
#include "cpu-gb.h"
#include "fastmem.h"
#include "profiler.h"

#define CPU_MEMORY_ACCESS() do { \
    if (cpu.mem_access_active) \
        cpu_mem_access(); \
} while (0)

byte readb(int a)
{
    byte *p = mbc.rmap[a>>12];
    CPU_MEMORY_ACCESS();
    if (p) return p[a];
    rockboy_profile_count(ROCKBOY_EVENT_SLOW_MEM_READS, 1);
    return mem_read(a);
}

void writeb(int a, byte b)
{
    byte *p = mbc.wmap[a>>12];
    CPU_MEMORY_ACCESS();
    if (p) p[a] = b;
    else
    {
        rockboy_profile_count(ROCKBOY_EVENT_SLOW_MEM_WRITES, 1);
        mem_write(a, b);
    }
}

int readw(int a)
{
    CPU_MEMORY_ACCESS();
    CPU_MEMORY_ACCESS();
    if ((a+1) & 0xfff)
    {
        byte *p = mbc.rmap[a>>12];
        if (p)
        {
#ifdef ROCKBOX_LITTLE_ENDIAN
#ifndef ALLOW_UNALIGNED_IO
            if (a&1) return p[a] | (p[a+1]<<8);
#endif
            return *(word *)(p+a);
#else
            return p[a] | (p[a+1]<<8);
#endif
        }
    }
    rockboy_profile_count(ROCKBOY_EVENT_SLOW_MEM_READS, 2);
    return mem_read(a) | (mem_read(a+1)<<8);
}

void writew(int a, int w)
{
    CPU_MEMORY_ACCESS();
    CPU_MEMORY_ACCESS();
    if ((a+1) & 0xfff)
    {
        byte *p = mbc.wmap[a>>12];
        if (p)
        {
#ifdef ROCKBOX_LITTLE_ENDIAN
#ifndef ALLOW_UNALIGNED_IO
            if (a&1)
            {
                p[a] = w;
                p[a+1] = w >> 8;
                return;
            }
#endif
            *(word *)(p+a) = w;
            return;
#else
            p[a] = w;
            p[a+1] = w >> 8;
            return;
#endif
        }
    }
    rockboy_profile_count(ROCKBOY_EVENT_SLOW_MEM_WRITES, 2);
    mem_write(a, w);
    mem_write(a+1, w>>8);
}

byte readhi(int a)
{
    return readb(a | 0xff00);
}

void writehi(int a, byte b)
{
    writeb(a | 0xff00, b);
}
