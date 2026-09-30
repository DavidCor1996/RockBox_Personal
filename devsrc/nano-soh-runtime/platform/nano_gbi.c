/* C adaptation of pinned Shipwright GbiWrap.cpp. This encodes commands;
 * native GLES interpretation is a separate required backend. */
#include "z64.h"
#include "soh/ResourceManagerHelpers.h"
#include <string.h>

char *ResourceMgr_LoadIfDListByName(char *path);

void gSPSegment(void *value, int segNum, uintptr_t target)
{
    if (ResourceMgr_OTRSigCheck((char *)target)) {
        uintptr_t loaded = (uintptr_t)ResourceMgr_LoadIfDListByName((char *)target);
        if (loaded) target = loaded;
    }
    __gSPSegment(value, segNum, target);
}

void gSPSegmentLoadRes(void *value, int segNum, uintptr_t target)
{
    if (ResourceMgr_OTRSigCheck((char *)target))
        target = (uintptr_t)ResourceMgr_LoadTexOrDListByName((char *)target);
    __gSPSegment(value, segNum, target);
}

void gSPDisplayList(Gfx *pkt, Gfx *dl)
{
    if (ResourceMgr_OTRSigCheck((char *)dl))
        dl = ResourceMgr_LoadGfxByName((char *)dl);
    __gSPDisplayList(pkt, dl);
}

void gSPDisplayListOffset(Gfx *pkt, Gfx *dl, int offset)
{
    if (ResourceMgr_OTRSigCheck((char *)dl))
        dl = ResourceMgr_LoadGfxByName((char *)dl);
    __gSPDisplayList(pkt, dl + offset);
}

void gSPVertex(Gfx *pkt, uintptr_t vertices, int n, int v0)
{
    if (ResourceMgr_OTRSigCheck((char *)vertices))
        vertices = (uintptr_t)ResourceMgr_LoadVtxByName((char *)vertices);
    __gSPVertex(pkt, vertices, n, v0);
}

void gSPInvalidateTexCache(Gfx *pkt, uintptr_t texture)
{
    if (texture && ResourceMgr_OTRSigCheck((char *)texture))
        texture = (uintptr_t)ResourceMgr_LoadTexOrDListByName((char *)texture);
    __gSPInvalidateTexCache(pkt, texture);
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

void gDPSetTileSizeInterp(Gfx *pkt, int tile, float uls, float ult,
                         float lrs, float lrt)
{
    __gDPSetTileSizeInterp(pkt, tile, 0, 0, 0, 0);
    pkt->words.w0 = _SHIFTL(G_SETTILESIZE_INTERP, 24, 8);
    pkt[1].words.w0 = float_bits(uls);
    pkt[1].words.w1 = float_bits(ult);
    pkt[2].words.w0 = float_bits(lrs);
    pkt[2].words.w1 = float_bits(lrt);
}

void gDPSetTileSizeLerp(Gfx *pkt, int tile, float uls0, float ult0,
                       float lrs0, float lrt0, float uls1, float ult1,
                       float lrs1, float lrt1)
{
    pkt[0].words.w0 = _SHIFTL(G_SETTILESIZE_LERP, 24, 8);
    pkt[0].words.w1 = _SHIFTL(tile, 24, 3);
    pkt[1].words.w0 = float_bits(uls0);
    pkt[1].words.w1 = float_bits(ult0);
    pkt[2].words.w0 = float_bits(lrs0);
    pkt[2].words.w1 = float_bits(lrt0);
    pkt[3].words.w0 = float_bits(uls1);
    pkt[3].words.w1 = float_bits(ult1);
    pkt[4].words.w0 = float_bits(lrs1);
    pkt[4].words.w1 = float_bits(lrt1);
}
