/* Original Skull Kid event sounds, decoded offline from its verified SWF.
 * All samples load before use; the mixer callback never allocates or reads disk.
 */
class SkullKidSoundHandler : public SilentSoundHandler
{
    struct Sample { short *data; int frames; int volume; } samples[8];
    struct Voice { int sample, position, loops; bool active; } voices[12];
    short blocks[2][512*2];
    int next_block, master_volume;
    unsigned int old_frequency;
    bool started;
    static SkullKidSoundHandler *current;
    static void more(const void **data, size_t *size)
    {
        SkullKidSoundHandler *self=current;
        if(!self) { *data=NULL; *size=0; return; }
        short *out=self->blocks[self->next_block];
        self->next_block^=1;
        for(int i=0;i<512;i++) {
            int left=0,right=0;
            for(int v=0;v<12;v++) {
                Voice &voice=self->voices[v];
                if(!voice.active) continue;
                Sample &sample=self->samples[voice.sample];
                if(voice.position>=sample.frames) {
                    if(voice.loops>0) {voice.loops--; voice.position=0;}
                    else {voice.active=false; continue;}
                }
                int volume=sample.volume*self->master_volume;
                left+=sample.data[voice.position*2]*volume/10000;
                right+=sample.data[voice.position*2+1]*volume/10000;
                voice.position++;
            }
            out[i*2]=(short)MAX(-32768,MIN(32767,left));
            out[i*2+1]=(short)MAX(-32768,MIN(32767,right));
        }
        *data=out; *size=sizeof(self->blocks[0]);
    }
public:
    SkullKidSoundHandler() : next_block(0), master_volume(100), started(false)
    {
        rb->memset(samples,0,sizeof(samples));
        rb->memset(voices,0,sizeof(voices));
        old_frequency=rb->mixer_get_frequency();
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        rb->mixer_set_frequency(44100);
        rb->pcmbuf_fade(false,true);
        current=this;
    }
    int load_event(int id)
    {
        static const int ids[]={19,20,184,438,656,734,744,781};
        int index=-1;
        for(int i=0;i<8;i++) if(ids[i]==id) index=i;
        if(index<0) return 0;
        char path[MAX_PATH];
        rb->snprintf(path,sizeof(path),FLASH_DIR "/skullkid/%d.pcm",id);
        int fd=rb->open(path,O_RDONLY);
        if(fd<0) {flash_logf("skull sound missing id=%d",id); return 0;}
        long bytes=rb->filesize(fd);
        if(bytes<=0 || bytes>2*1024*1024 || bytes%4) {rb->close(fd);return 0;}
        short *data=new short[bytes/2];
        if(!data) {rb->close(fd);return 0;}
        int got=rb->read(fd,data,bytes);
        rb->close(fd);
        if(got!=bytes) {delete[] data;return 0;}
        samples[index].data=data;
        samples[index].frames=bytes/4;
        samples[index].volume=100;
        flash_logf("skull sound id=%d frames=%d",id,(int)bytes/4);
        return index+1;
    }
    virtual void play_sound(gameswf::as_object *,int handle,int loops)
    {
        if(handle<1 || handle>8 || !samples[handle-1].data) return;
        rb->pcm_play_lock();
        int slot=-1;
        for(int i=0;i<12;i++) if(!voices[i].active) {slot=i;break;}
        if(slot>=0) {
            voices[slot].sample=handle-1; voices[slot].position=0;
            voices[slot].loops=MAX(0,loops); voices[slot].active=true;
        }
        rb->pcm_play_unlock();
        if(!started) {
            rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,MIX_AMP_UNITY);
            rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,more,NULL,0);
            started=true;
        }
    }
    virtual void stop_sound(int handle)
    {
        rb->pcm_play_lock();
        for(int i=0;i<12;i++) if(voices[i].sample==handle-1) voices[i].active=false;
        rb->pcm_play_unlock();
    }
    virtual void stop_all_sounds()
    {
        rb->pcm_play_lock();
        for(int i=0;i<12;i++) voices[i].active=false;
        rb->pcm_play_unlock();
    }
    virtual void set_volume(int handle,int volume)
    {
        if(handle>=1 && handle<=8) samples[handle-1].volume=MAX(0,MIN(100,volume));
    }
    virtual void set_max_volume(int volume) {master_volume=MAX(0,MIN(100,volume));}
    /* Definition destruction may precede the last mixer callback. Ownership is
     * retained until the handler stops output and destroys all samples. */
    virtual void delete_sound(int handle) {stop_sound(handle);}
    void close()
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        current=NULL; started=false;
    }
    virtual ~SkullKidSoundHandler()
    {
        close();
        for(int i=0;i<8;i++) delete[] samples[i].data;
        rb->pcmbuf_fade(false,false);
        rb->mixer_set_frequency(old_frequency);
    }
};
SkullKidSoundHandler *SkullKidSoundHandler::current;
