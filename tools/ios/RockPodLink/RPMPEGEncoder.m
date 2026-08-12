#import "RPMPEGEncoder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>

static const int RPMPEGWidth = 320;
static const int RPMPEGHeight = 240;
static const int RPMPEGFPS = 20;
static const int RPMPEGSampleRate = 44100;

static NSString *RPMPEGError(int code, NSString *fallback)
{
    char detail[AV_ERROR_MAX_STRING_SIZE] = {0};
    if (code < 0 && av_strerror(code, detail, sizeof(detail)) == 0)
        return [NSString stringWithFormat:@"%@: %s", fallback, detail];
    return fallback;
}

@implementation RPMPEGEncoder
{
    NSURL *_outputURL;
    AVFormatContext *_format;
    AVCodecContext *_videoCodec;
    AVCodecContext *_audioCodec;
    AVStream *_videoStream;
    AVStream *_audioStream;
    AVFrame *_videoFrame;
    AVFrame *_audioFrame;
    AVPacket *_packet;
    BOOL _finished;
}

- (instancetype)initWithURL:(NSURL *)url error:(NSString **)error
{
    self = [super init];
    if (!self)
        return nil;
    _outputURL = url;
    int result = avformat_alloc_output_context2(&_format, NULL, "mpeg", url.fileSystemRepresentation);
    if (result < 0 || !_format)
        goto fail_format;
    _format->packet_size = 2048;

    const AVCodec *video = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    const AVCodec *audio = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!video || !audio)
    {
        if (error) *error = @"The bundled MPEG-2 encoder is unavailable.";
        [self cancel];
        return nil;
    }

    _videoStream = avformat_new_stream(_format, NULL);
    _audioStream = avformat_new_stream(_format, NULL);
    _videoCodec = avcodec_alloc_context3(video);
    _audioCodec = avcodec_alloc_context3(audio);
    if (!_videoStream || !_audioStream || !_videoCodec || !_audioCodec)
    {
        if (error) *error = @"There was not enough memory to start MPEG conversion.";
        [self cancel];
        return nil;
    }

    _videoCodec->codec_id = AV_CODEC_ID_MPEG2VIDEO;
    _videoCodec->codec_type = AVMEDIA_TYPE_VIDEO;
    _videoCodec->width = RPMPEGWidth;
    _videoCodec->height = RPMPEGHeight;
    _videoCodec->pix_fmt = AV_PIX_FMT_YUV420P;
    _videoCodec->time_base = (AVRational){1, RPMPEGFPS};
    _videoCodec->framerate = (AVRational){RPMPEGFPS, 1};
    _videoCodec->gop_size = 12;
    _videoCodec->max_b_frames = 0;
    _videoCodec->bit_rate = 1600000;
    _videoCodec->rc_max_rate = 1600000;
    _videoCodec->rc_buffer_size = 800000;
    _videoCodec->flags |= AV_CODEC_FLAG_LOW_DELAY | AV_CODEC_FLAG_QSCALE;
    _videoCodec->global_quality = FF_QP2LAMBDA * 2;
    _videoCodec->thread_count = 1;
    av_opt_set_int(_videoCodec, "sc_threshold", 0, 0);
    _videoStream->time_base = _videoCodec->time_base;
    result = avcodec_open2(_videoCodec, video, NULL);
    if (result < 0)
        goto fail_video;
    result = avcodec_parameters_from_context(_videoStream->codecpar, _videoCodec);
    if (result < 0)
        goto fail_video;

    _audioCodec->codec_id = AV_CODEC_ID_MP2;
    _audioCodec->codec_type = AVMEDIA_TYPE_AUDIO;
    _audioCodec->sample_rate = RPMPEGSampleRate;
    _audioCodec->sample_fmt = AV_SAMPLE_FMT_S16;
    _audioCodec->bit_rate = 112000;
    _audioCodec->time_base = (AVRational){1, RPMPEGSampleRate};
    av_channel_layout_default(&_audioCodec->ch_layout, 2);
    _audioStream->time_base = _audioCodec->time_base;
    result = avcodec_open2(_audioCodec, audio, NULL);
    if (result < 0)
        goto fail_audio;
    result = avcodec_parameters_from_context(_audioStream->codecpar, _audioCodec);
    if (result < 0)
        goto fail_audio;

    _videoFrame = av_frame_alloc();
    _audioFrame = av_frame_alloc();
    _packet = av_packet_alloc();
    if (!_videoFrame || !_audioFrame || !_packet)
    {
        if (error) *error = @"There was not enough memory to buffer MPEG conversion.";
        [self cancel];
        return nil;
    }
    _videoFrame->format = _videoCodec->pix_fmt;
    _videoFrame->width = _videoCodec->width;
    _videoFrame->height = _videoCodec->height;
    result = av_frame_get_buffer(_videoFrame, 32);
    if (result < 0)
        goto fail_buffer;
    _audioFrame->format = _audioCodec->sample_fmt;
    _audioFrame->sample_rate = _audioCodec->sample_rate;
    _audioFrame->nb_samples = _audioCodec->frame_size;
    av_channel_layout_copy(&_audioFrame->ch_layout, &_audioCodec->ch_layout);
    result = av_frame_get_buffer(_audioFrame, 0);
    if (result < 0)
        goto fail_buffer;

    result = avio_open(&_format->pb, url.fileSystemRepresentation, AVIO_FLAG_WRITE);
    if (result < 0)
        goto fail_open;
    result = avformat_write_header(_format, NULL);
    if (result < 0)
        goto fail_header;
    return self;

fail_header:
    if (error) *error = RPMPEGError(result, @"The MPEG program-stream header could not be written");
    [self cancel]; return nil;
fail_open:
    if (error) *error = RPMPEGError(result, @"The MPEG output file could not be opened");
    [self cancel]; return nil;
fail_buffer:
    if (error) *error = RPMPEGError(result, @"The MPEG frame buffers could not be allocated");
    [self cancel]; return nil;
fail_audio:
    if (error) *error = RPMPEGError(result, @"The MP2 audio encoder could not start");
    [self cancel]; return nil;
fail_video:
    if (error) *error = RPMPEGError(result, @"The MPEG-2 video encoder could not start");
    [self cancel]; return nil;
fail_format:
    if (error) *error = RPMPEGError(result, @"The MPEG program-stream writer is unavailable");
    [self cancel]; return nil;
}

- (NSInteger)audioFrameSamples
{
    return _audioCodec ? _audioCodec->frame_size : 1152;
}

- (BOOL)drainCodec:(AVCodecContext *)codec stream:(AVStream *)stream error:(NSString **)error
{
    for (;;)
    {
        int result = avcodec_receive_packet(codec, _packet);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return YES;
        if (result < 0)
        {
            if (error) *error = RPMPEGError(result, @"An encoded MPEG packet could not be read");
            return NO;
        }
        av_packet_rescale_ts(_packet, codec->time_base, stream->time_base);
        _packet->stream_index = stream->index;
        result = av_interleaved_write_frame(_format, _packet);
        av_packet_unref(_packet);
        if (result < 0)
        {
            if (error) *error = RPMPEGError(result, @"An MPEG packet could not be written");
            return NO;
        }
    }
}

- (BOOL)appendVideoPixelBuffer:(CVPixelBufferRef)buffer
                     frameIndex:(int64_t)frameIndex
                          error:(NSString **)error
{
    int result = av_frame_make_writable(_videoFrame);
    if (result < 0)
    {
        if (error) *error = RPMPEGError(result, @"The MPEG video frame could not be prepared");
        return NO;
    }
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    const uint8_t *bgra = CVPixelBufferGetBaseAddress(buffer);
    size_t stride = CVPixelBufferGetBytesPerRow(buffer);
    for (int y = 0; y < RPMPEGHeight; y++)
    {
        uint8_t *yRow = _videoFrame->data[0] + y * _videoFrame->linesize[0];
        for (int x = 0; x < RPMPEGWidth; x++)
        {
            const uint8_t *p = bgra + y * stride + x * 4;
            int b = p[0], g = p[1], r = p[2];
            yRow[x] = (uint8_t)MAX(0, MIN(255,
                ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16));
        }
    }
    for (int y = 0; y < RPMPEGHeight; y += 2)
    {
        uint8_t *uRow = _videoFrame->data[1] + (y / 2) * _videoFrame->linesize[1];
        uint8_t *vRow = _videoFrame->data[2] + (y / 2) * _videoFrame->linesize[2];
        for (int x = 0; x < RPMPEGWidth; x += 2)
        {
            int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                {
                    const uint8_t *p = bgra + (y + dy) * stride + (x + dx) * 4;
                    b += p[0]; g += p[1]; r += p[2];
                }
            r /= 4; g /= 4; b /= 4;
            uRow[x / 2] = (uint8_t)MAX(0, MIN(255,
                ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128));
            vRow[x / 2] = (uint8_t)MAX(0, MIN(255,
                ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128));
        }
    }
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    _videoFrame->pts = frameIndex;
    result = avcodec_send_frame(_videoCodec, _videoFrame);
    if (result < 0)
    {
        if (error) *error = RPMPEGError(result, @"The MPEG-2 encoder rejected a video frame");
        return NO;
    }
    return [self drainCodec:_videoCodec stream:_videoStream error:error];
}

- (BOOL)appendAudioPCMData:(NSData *)pcm
                sampleIndex:(int64_t)sampleIndex
                      error:(NSString **)error
{
    NSUInteger expected = (NSUInteger)self.audioFrameSamples * 2 * sizeof(int16_t);
    if (pcm.length != expected)
    {
        if (error) *error = @"The MP2 encoder received an incomplete audio frame.";
        return NO;
    }
    int result = av_frame_make_writable(_audioFrame);
    if (result < 0)
    {
        if (error) *error = RPMPEGError(result, @"The MP2 audio frame could not be prepared");
        return NO;
    }
    memcpy(_audioFrame->data[0], pcm.bytes, pcm.length);
    _audioFrame->pts = sampleIndex;
    result = avcodec_send_frame(_audioCodec, _audioFrame);
    if (result < 0)
    {
        if (error) *error = RPMPEGError(result, @"The MP2 encoder rejected an audio frame");
        return NO;
    }
    return [self drainCodec:_audioCodec stream:_audioStream error:error];
}

- (BOOL)finishWithError:(NSString **)error
{
    if (_finished)
        return YES;
    int result = avcodec_send_frame(_videoCodec, NULL);
    if (result < 0 || ![self drainCodec:_videoCodec stream:_videoStream error:error])
        return NO;
    result = avcodec_send_frame(_audioCodec, NULL);
    if (result < 0 || ![self drainCodec:_audioCodec stream:_audioStream error:error])
        return NO;
    result = av_write_trailer(_format);
    if (result < 0)
    {
        if (error) *error = RPMPEGError(result, @"The MPEG program stream could not be finalized");
        return NO;
    }
    _finished = YES;
    if (_format && _format->pb)
        avio_closep(&_format->pb);
    return YES;
}

- (void)cancel
{
    if (_format && _format->pb)
        avio_closep(&_format->pb);
    av_frame_free(&_videoFrame);
    av_frame_free(&_audioFrame);
    av_packet_free(&_packet);
    avcodec_free_context(&_videoCodec);
    avcodec_free_context(&_audioCodec);
    avformat_free_context(_format);
    _format = NULL;
    if (!_finished && _outputURL)
        [NSFileManager.defaultManager removeItemAtURL:_outputURL error:nil];
}

- (void)dealloc
{
    [self cancel];
}

@end
