/***************************************************************************
 * The iPod build intentionally uses the original SPC soundtrack only.
 * Zelda3's optional MSU-1 music path still references the Opus API even
 * when disabled, so provide inert symbols instead of linking a large,
 * floating-point decoder that can never be selected on this platform.
 ****************************************************************************/
#include "zelda3.h"
#include "upstream/third_party/opus-1.3.1-stripped/opus.h"

int opus_decoder_get_size(int channels)
{
    (void)channels;
    return 0;
}

OpusDecoder *opus_decoder_create(opus_int32 rate, int channels, int *error)
{
    (void)rate;
    (void)channels;
    if (error)
        *error = OPUS_UNIMPLEMENTED;
    return NULL;
}

int opus_decoder_init(OpusDecoder *decoder, opus_int32 rate, int channels)
{
    (void)decoder;
    (void)rate;
    (void)channels;
    return OPUS_UNIMPLEMENTED;
}

int opus_decode(OpusDecoder *decoder, const unsigned char *data,
                opus_int32 length, opus_int16 *pcm, int frame_size,
                int decode_fec)
{
    (void)decoder;
    (void)data;
    (void)length;
    (void)pcm;
    (void)frame_size;
    (void)decode_fec;
    return OPUS_UNIMPLEMENTED;
}

int opus_decoder_ctl(OpusDecoder *decoder, int request, ...)
{
    (void)decoder;
    (void)request;
    return OPUS_UNIMPLEMENTED;
}

void opus_decoder_destroy(OpusDecoder *decoder)
{
    (void)decoder;
}
