/* Bounded RAM diagnostics, exported after media callbacks have stopped. */
#ifndef VIDEO_METRICS_H
#define VIDEO_METRICS_H
#define VIDEO_TRACE_COUNT 128
static struct {
    uint32_t pts, clock, read_us, decode_us, present_us;
    uint16_t width, height;
    unsigned char presented, audio_master;
} video_trace[VIDEO_TRACE_COUNT];
static unsigned video_trace_head, video_trace_total, video_trace_drops;
static uint32_t video_trace_max_late, video_trace_max_read;
static uint32_t video_trace_max_decode, video_trace_max_present;
static size_t video_trace_pool_reserved;
static void video_trace_export(void)
{
    int fd = open(ROCKBOX_DIR "/video-playback-trace.tsv",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd < 0) return;
    fdprintf(fd,"codec=h264 decoder=VPU ready_queue_max=1 lcd=320x240 "
        "tv_input=native tv_higher_resolution=unqualified "
        "clock=mixer_consumed analog_latency=unmeasured "
        "frames=%u dropped_presentations=%u audio_underruns=%lu\n",
        video_trace_total,video_trace_drops,(unsigned long)video_pcm_underruns());
    fdprintf(fd,"pts_ms\tclock_ms\tread_us\tdecode_us\tpresent_us\twidth\theight\tpresented\taudio_master\n");
    fdprintf(fd,"# shared_pool_reserved=%lu read_buffer=%u pcm_ring=262144 "
        "max_lateness_ms=%lu max_read_us=%lu max_decode_us=%lu "
        "max_combined_tv_lcd_present_us=%lu tv_fallback=320x240 "
        "field_cadence=unmeasured memory_scope=player_pool_plus_named_static_buffers\n",
        (unsigned long)video_trace_pool_reserved,VIDEO_CAP_SAMPLE_BUFFER_BYTES,
        (unsigned long)video_trace_max_late,(unsigned long)video_trace_max_read,
        (unsigned long)video_trace_max_decode,(unsigned long)video_trace_max_present);
    unsigned count = MIN(video_trace_total, VIDEO_TRACE_COUNT);
    for (unsigned n=0;n<count;n++)
    {
        unsigned i = (video_trace_head+VIDEO_TRACE_COUNT-count+n)%VIDEO_TRACE_COUNT;
        fdprintf(fd,"%lu\t%lu\t%lu\t%lu\t%lu\t%u\t%u\t%u\t%u\n",
            (unsigned long)video_trace[i].pts,(unsigned long)video_trace[i].clock,
            (unsigned long)video_trace[i].read_us,(unsigned long)video_trace[i].decode_us,
            (unsigned long)video_trace[i].present_us,video_trace[i].width,
            video_trace[i].height,video_trace[i].presented,video_trace[i].audio_master);
    }
    close(fd);
}
#endif
