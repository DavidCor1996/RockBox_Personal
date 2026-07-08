/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp

extern "C" {
#include <setjmp.h>
}

#include "lib/plugin_cxx_compat.h"
#include "upstream_bridge.h"
#include "queen_loader.h"
#include "sky_loader.h"
#include "sky_runtime.h"

#include "audio/mixer.h"
#include "audio/audiostream.h"
#include "audio/timestamp.h"
#include "backends/fs/fs-factory.h"
#include "backends/fs/abstract-fs.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/timer.h"
#include "common/util.h"
#include "graphics/palette.h"
#include "graphics/surface.h"
#include "queen/queen.h"

struct RockboxStreamTrace {
    int fd;
    char path[MAX_PATH];
};

static RockboxStreamTrace stream_trace[16];
static jmp_buf upstream_fatal_jmp;
static bool upstream_fatal_armed;
static char upstream_fatal_message[192];

static void upstream_status_screen(const char *line1, const char *line2)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(4, 12, "ScummVM upstream");
    rb->lcd_putsxy(4, 34, line1 ? line1 : "");
    if (line2)
        rb->lcd_putsxy(4, 52, line2);
    rb->lcd_update();
}

extern "C" void scummvm_upstream_fatal_error(const char *msg)
{
    rb->strlcpy(upstream_fatal_message, msg ? msg : "ScummVM fatal error",
                sizeof(upstream_fatal_message));
    DEBUGF("scummvm: upstream fatal: %s\n", upstream_fatal_message);
    if (upstream_fatal_armed)
        longjmp(upstream_fatal_jmp, 1);
    while (true)
        rb->sleep(HZ);
}

static void trace_stream_open(int fd, const char *path)
{
    if (fd < 0)
        return;
    for (int i = 0; i < (int)ARRAYLEN(stream_trace); i++) {
        if (stream_trace[i].fd < 0 || stream_trace[i].path[0] == '\0') {
            stream_trace[i].fd = fd;
            rb->strlcpy(stream_trace[i].path, path, sizeof(stream_trace[i].path));
            DEBUGF("scummvm: stream open fd=%d path=%s\n", fd, path);
            return;
        }
    }
    DEBUGF("scummvm: stream open fd=%d path=%s trace-full\n", fd, path);
}

static void trace_stream_close(int fd)
{
    if (fd < 0)
        return;
    for (int i = 0; i < (int)ARRAYLEN(stream_trace); i++) {
        if (stream_trace[i].fd == fd && stream_trace[i].path[0] != '\0') {
            DEBUGF("scummvm: stream close fd=%d path=%s\n", fd,
                   stream_trace[i].path);
            stream_trace[i].fd = -1;
            stream_trace[i].path[0] = '\0';
            return;
        }
    }
    DEBUGF("scummvm: stream close fd=%d untracked\n", fd);
}

static void trace_stream_dump_open(void)
{
    for (int i = 0; i < (int)ARRAYLEN(stream_trace); i++) {
        if (stream_trace[i].fd >= 0 && stream_trace[i].path[0] != '\0')
            DEBUGF("scummvm: stream leaked fd=%d path=%s\n",
                   stream_trace[i].fd, stream_trace[i].path);
    }
}

class RockboxPaletteManager : public PaletteManager {
public:
    RockboxPaletteManager()
    {
        rb->memset(_colors, 0, sizeof(_colors));
    }

    void setPalette(const byte *colors, uint start, uint num)
    {
        if (start >= 256)
            return;
        if (start + num > 256)
            num = 256 - start;
        rb->memcpy(&_colors[start * 3], colors, num * 3);
    }

    void grabPalette(byte *colors, uint start, uint num)
    {
        if (start >= 256)
            return;
        if (start + num > 256)
            num = 256 - start;
        rb->memcpy(colors, &_colors[start * 3], num * 3);
    }

    fb_data color(byte index) const
    {
        const byte *c = &_colors[index * 3];
        return LCD_RGBPACK(c[0], c[1], c[2]);
    }

private:
    byte _colors[256 * 3];
};

class RockboxReadStream : public Common::SeekableReadStream {
public:
    RockboxReadStream(int fd, const char *path) : _fd(fd), _eos(false),
        _err(false)
    {
        _size = rb->filesize(fd);
        trace_stream_open(fd, path);
    }

    ~RockboxReadStream()
    {
        if (_fd >= 0) {
            trace_stream_close(_fd);
            rb->close(_fd);
        }
    }

    bool err() const { return _err; }
    void clearErr() { _err = false; _eos = false; }
    bool eos() const { return _eos; }
    uint32 read(void *dataPtr, uint32 dataSize)
    {
        ssize_t got = rb->read(_fd, dataPtr, dataSize);
        if (got < 0) {
            _err = true;
            return 0;
        }
        if ((uint32)got < dataSize)
            _eos = true;
        return (uint32)got;
    }
    int32 pos() const { return rb->lseek(_fd, 0, SEEK_CUR); }
    int32 size() const { return _size; }
    bool seek(int32 offset, int whence = SEEK_SET)
    {
        if (rb->lseek(_fd, offset, whence) < 0) {
            _err = true;
            return false;
        }
        _eos = false;
        return true;
    }

private:
    int _fd;
    int32 _size;
    bool _eos;
    bool _err;
};

class RockboxWriteStream : public Common::WriteStream {
public:
    RockboxWriteStream(int fd, const char *path) : _fd(fd), _err(false)
    {
        trace_stream_open(fd, path);
    }
    ~RockboxWriteStream()
    {
        if (_fd >= 0) {
            trace_stream_close(_fd);
            rb->close(_fd);
        }
    }

    bool err() const { return _err; }
    void clearErr() { _err = false; }
    uint32 write(const void *dataPtr, uint32 dataSize)
    {
        ssize_t written = rb->write(_fd, dataPtr, dataSize);
        if (written < 0) {
            _err = true;
            return 0;
        }
        return (uint32)written;
    }
    bool flush() { return true; }
    int32 pos() const { return rb->lseek(_fd, 0, SEEK_CUR); }

private:
    int _fd;
    bool _err;
};

class RockboxFSNode : public AbstractFSNode {
public:
    RockboxFSNode(const Common::String &path) : _path(path)
    {
    }

    bool exists() const
    {
        if (rb->dir_exists(_path.c_str()))
            return true;
        int fd = rb->open(_path.c_str(), O_RDONLY);
        if (fd < 0)
            return false;
        rb->close(fd);
        return true;
    }
    bool getChildren(AbstractFSList &list, ListMode mode, bool hidden) const
    {
        DIR *dir = rb->opendir(_path.c_str());
        struct dirent *entry;
        if (!dir)
            return false;

        while ((entry = rb->readdir(dir))) {
            Common::String name(entry->d_name);
            if (!hidden && !name.empty() && name[0] == '.')
                continue;
            Common::String path = _path;
            if (!path.empty() && path.lastChar() != '/')
                path += '/';
            path += name;
            bool isDir = rb->dir_exists(path.c_str());
            if ((mode == Common::FSNode::kListFilesOnly && isDir) ||
                (mode == Common::FSNode::kListDirectoriesOnly && !isDir))
                continue;
            list.push_back(new RockboxFSNode(path));
        }

        rb->closedir(dir);
        return true;
    }
    Common::String getName() const
    {
        const char *name = rb->strrchr(_path.c_str(), '/');
        return name ? Common::String(name + 1) : _path;
    }
    Common::String getPath() const { return _path; }
    bool isDirectory() const { return rb->dir_exists(_path.c_str()); }
    bool isReadable() const { return exists(); }
    bool isWritable() const { return true; }
    Common::SeekableReadStream *createReadStream()
    {
        int fd = rb->open(_path.c_str(), O_RDONLY);
        if (fd < 0)
            return 0;
        return new RockboxReadStream(fd, _path.c_str());
    }
    Common::WriteStream *createWriteStream()
    {
        int fd = rb->open(_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
            return 0;
        return new RockboxWriteStream(fd, _path.c_str());
    }
    bool create(bool isDirectory)
    {
        if (isDirectory)
            return (rb->mkdir)(_path.c_str()) == 0 ||
                   rb->dir_exists(_path.c_str());
        int fd = rb->open(_path.c_str(), O_WRONLY | O_CREAT, 0666);
        if (fd < 0)
            return false;
        rb->close(fd);
        return true;
    }

protected:
    AbstractFSNode *getChild(const Common::String &name) const
    {
        Common::String path = _path;
        if (!path.empty() && path.lastChar() != '/')
            path += '/';
        path += name;
        return new RockboxFSNode(path);
    }
    AbstractFSNode *getParent() const
    {
        Common::String path = _path;
        while (path.size() > 1 && path.lastChar() == '/')
            path.deleteLastChar();
        const char *last = rb->strrchr(path.c_str(), '/');
        if (!last || last == path.c_str())
            return new RockboxFSNode("/");
        path.erase(last - path.c_str());
        return new RockboxFSNode(path);
    }

private:
    Common::String _path;
};

class RockboxFilesystemFactory : public FilesystemFactory {
public:
    AbstractFSNode *makeRootFileNode() const
    {
        return new RockboxFSNode("/");
    }
    AbstractFSNode *makeCurrentDirectoryFileNode() const
    {
        return new RockboxFSNode("/");
    }
    AbstractFSNode *makeFileNodePath(const Common::String &path) const
    {
        return new RockboxFSNode(path);
    }
};

class RockboxEventManager : public Common::EventManager {
public:
    RockboxEventManager() : _mouse(160, 100), _buttons(0), _quit(false),
        _rtl(false), _lastDown(false) {}

    void setInput(int x, int y, bool down, bool clicked)
    {
        Common::Event ev;
        if (_mouse.x != x || _mouse.y != y) {
            _mouse = Common::Point(x, y);
            ev.type = Common::EVENT_MOUSEMOVE;
            ev.mouse = _mouse;
            pushEvent(ev);
        }
        if ((down && !_lastDown) || clicked) {
            ev = Common::Event();
            ev.type = Common::EVENT_LBUTTONDOWN;
            ev.mouse = _mouse;
            pushEvent(ev);
            _buttons |= LBUTTON;
        }
        if ((!down && _lastDown) || clicked) {
            ev = Common::Event();
            ev.type = Common::EVENT_LBUTTONUP;
            ev.mouse = _mouse;
            pushEvent(ev);
            _buttons &= ~LBUTTON;
        }
        _lastDown = down && !clicked;
    }

    bool pollEvent(Common::Event &event)
    {
        if (_queue.empty())
            return false;
        event = _queue.pop();
        if (event.type == Common::EVENT_QUIT)
            _quit = true;
        return true;
    }

    void pushEvent(const Common::Event &event) { _queue.push(event); }
    Common::Point getMousePos() const { return _mouse; }
    int getButtonState() const { return _buttons; }
    int getModifierState() const { return 0; }
    int shouldQuit() const { return _quit; }
    int shouldRTL() const { return _rtl; }
    void resetRTL() { _rtl = false; }

private:
    Common::Queue<Common::Event> _queue;
    Common::Point _mouse;
    int _buttons;
    bool _quit;
    bool _rtl;
    bool _lastDown;
};

class RockboxTimerManager : public Common::TimerManager {
public:
    bool installTimerProc(TimerProc proc, int32 interval, void *refCon,
                          const Common::String &id)
    {
        (void)proc;
        (void)interval;
        (void)refCon;
        (void)id;
        return true;
    }

    void removeTimerProc(TimerProc proc) { (void)proc; }
};

class RockboxMixer : public Audio::Mixer {
public:
    RockboxMixer() : _nextHandle(1), _outputRate(22050), _oldRate(0),
        _audioActive(false)
    {
        for (int i = 0; i < 4; i++) {
            _volume[i] = kMaxMixerVolume;
            _muted[i] = false;
        }
        for (int i = 0; i < MAX_CHANNELS; i++)
            clearChannel(i);
        activeMixer = this;
    }

    ~RockboxMixer()
    {
        stopAll();
        if (activeMixer == this)
            activeMixer = 0;
    }

    bool isReady() const { return true; }
    void playStream(SoundType type, Audio::SoundHandle *handle,
                    Audio::AudioStream *stream, int id = -1,
                    byte volume = kMaxChannelVolume, int8 balance = 0,
                    DisposeAfterUse::Flag autofreeStream = DisposeAfterUse::YES,
                    bool permanent = false, bool reverseStereo = false)
    {
        (void)permanent; (void)reverseStereo;
        int slot = findFreeChannel();
        if (!stream || slot < 0 || isSoundIDActive(id)) {
            delete stream;
            return;
        }

        if (!_audioActive) {
            _oldRate = rb->mixer_get_frequency();
            _outputRate = stream->getRate() > 0 ? stream->getRate() : 22050;
            rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
            rb->mixer_set_frequency(_outputRate);
            rb->pcmbuf_fade(false, true);
            _audioActive = true;
        }

        ChannelState &ch = _channels[slot];
        ch.stream = stream;
        ch.type = type;
        ch.id = id;
        ch.volume = volume;
        ch.balance = balance;
        ch.autofree = autofreeStream == DisposeAfterUse::YES;
        ch.paused = false;
        ch.handle = _nextHandle++;
        if (ch.handle == 0 || ch.handle == 0xffffffff)
            ch.handle = _nextHandle++;
        if (handle)
            setHandle(*handle, ch.handle);

        if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) ==
            CHANNEL_STOPPED)
            rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                        pcmGetMore, NULL, 0);
    }
    void stopAll()
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        for (int i = 0; i < MAX_CHANNELS; i++)
            releaseChannel(i);
        if (_audioActive) {
            rb->pcmbuf_fade(false, false);
            if (_oldRate)
                rb->mixer_set_frequency(_oldRate);
        }
        _audioActive = false;
        _oldRate = 0;
    }
    void stopID(int id)
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].id == id)
                releaseChannel(i);
        }
    }
    void stopHandle(Audio::SoundHandle handle)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                releaseChannel(i);
        }
    }
    void pauseAll(bool paused)
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream)
                _channels[i].paused = paused;
        }
    }
    void pauseID(int id, bool paused)
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].id == id)
                _channels[i].paused = paused;
        }
    }
    void pauseHandle(Audio::SoundHandle handle, bool paused)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                _channels[i].paused = paused;
        }
    }
    bool isSoundIDActive(int id)
    {
        if (id < 0)
            return false;
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].id == id)
                return true;
        }
        return false;
    }
    int getSoundID(Audio::SoundHandle handle)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                return _channels[i].id;
        }
        return -1;
    }
    bool isSoundHandleActive(Audio::SoundHandle handle)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                return true;
        }
        return false;
    }
    void muteSoundType(SoundType type, bool mute) { _muted[type] = mute; }
    bool isSoundTypeMuted(SoundType type) const { return _muted[type]; }
    void setChannelVolume(Audio::SoundHandle handle, byte volume)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                _channels[i].volume = volume;
        }
    }
    byte getChannelVolume(Audio::SoundHandle handle)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                return _channels[i].volume;
        }
        return kMaxChannelVolume;
    }
    void setChannelBalance(Audio::SoundHandle handle, int8 balance)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                _channels[i].balance = balance;
        }
    }
    int8 getChannelBalance(Audio::SoundHandle handle)
    {
        uint32 value = handleValue(handle);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].handle == value)
                return _channels[i].balance;
        }
        return 0;
    }
    uint32 getSoundElapsedTime(Audio::SoundHandle handle)
    {
        (void)handle; return 0;
    }
    Audio::Timestamp getElapsedTime(Audio::SoundHandle handle)
    {
        (void)handle; return Audio::Timestamp(0, getOutputRate());
    }
    bool hasActiveChannelOfType(SoundType type)
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (_channels[i].stream && _channels[i].type == type)
                return true;
        }
        return false;
    }
    void setVolumeForSoundType(SoundType type, int volume)
    {
        _volume[type] = volume;
    }
    int getVolumeForSoundType(SoundType type) const { return _volume[type]; }
    uint getOutputRate() const { return _outputRate; }

private:
    enum { MAX_CHANNELS = 6, BUFFER_FRAMES = 1024,
           BUFFER_SAMPLES = BUFFER_FRAMES * 2 };
    struct ChannelState {
        Audio::AudioStream *stream;
        SoundType type;
        int id;
        uint32 handle;
        byte volume;
        int8 balance;
        bool autofree;
        bool paused;
    };

    static RockboxMixer *activeMixer;
    static int16 pcmBuffer[BUFFER_SAMPLES];
    static int16 streamBuffer[BUFFER_SAMPLES];

    static uint32 handleValue(const Audio::SoundHandle &handle)
    {
        return *reinterpret_cast<const uint32 *>(&handle);
    }

    static void setHandle(Audio::SoundHandle &handle, uint32 value)
    {
        *reinterpret_cast<uint32 *>(&handle) = value;
    }

    void clearChannel(int slot)
    {
        _channels[slot].stream = 0;
        _channels[slot].type = kPlainSoundType;
        _channels[slot].id = -1;
        _channels[slot].handle = 0xffffffff;
        _channels[slot].volume = kMaxChannelVolume;
        _channels[slot].balance = 0;
        _channels[slot].autofree = true;
        _channels[slot].paused = false;
    }

    void releaseChannel(int slot)
    {
        Audio::AudioStream *stream = _channels[slot].stream;
        bool autofree = _channels[slot].autofree;
        clearChannel(slot);
        if (stream && autofree)
            delete stream;
    }

    int findFreeChannel() const
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (!_channels[i].stream)
                return i;
        }
        return -1;
    }

    static void pcmGetMore(const void **start, size_t *size)
    {
        if (!activeMixer) {
            *start = 0;
            *size = 0;
            return;
        }
        activeMixer->fillPcm(start, size);
    }

    void fillPcm(const void **start, size_t *size)
    {
        bool any = false;

        rb->memset(pcmBuffer, 0, sizeof(pcmBuffer));
        for (int i = 0; i < MAX_CHANNELS; i++) {
            ChannelState &ch = _channels[i];
            if (!ch.stream || ch.paused || _muted[ch.type])
                continue;

            bool stereo = ch.stream->isStereo();
            int wanted = stereo ? BUFFER_SAMPLES : BUFFER_FRAMES;
            int got = ch.stream->readBuffer(streamBuffer, wanted);
            if (got <= 0) {
                if (ch.stream->endOfStream())
                    releaseChannel(i);
                continue;
            }

            any = true;
            int typeVolume = _volume[ch.type];
            int scale = (int)ch.volume * typeVolume;
            int outSamples = stereo ? got : got * 2;
            for (int s = 0; s < outSamples; s++) {
                int src = stereo ? streamBuffer[s] : streamBuffer[s / 2];
                int mixed = pcmBuffer[s] + src * scale /
                    (kMaxChannelVolume * kMaxMixerVolume);
                if (mixed > 32767)
                    mixed = 32767;
                else if (mixed < -32768)
                    mixed = -32768;
                pcmBuffer[s] = (int16)mixed;
            }
        }

        if (!any) {
            *start = 0;
            *size = 0;
            return;
        }

        *start = pcmBuffer;
        *size = sizeof(pcmBuffer);
    }

    ChannelState _channels[MAX_CHANNELS];
    uint32 _nextHandle;
    uint _outputRate;
    unsigned int _oldRate;
    bool _audioActive;
    int _volume[4];
    bool _muted[4];
};

RockboxMixer *RockboxMixer::activeMixer;
int16 RockboxMixer::pcmBuffer[RockboxMixer::BUFFER_SAMPLES];
int16 RockboxMixer::streamBuffer[RockboxMixer::BUFFER_SAMPLES];

class RockboxSaveFileManager : public Common::SaveFileManager {
public:
    Common::OutSaveFile *openForSaving(const Common::String &name,
                                       bool compress = true)
    {
        (void)compress;
        Common::String path;

        if (!resolveSavePath(name, path, true))
            return 0;

        int fd = rb->open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            setError(Common::kWritingFailed, "Could not create save file");
            return 0;
        }

        return new Common::OutSaveFile(new RockboxWriteStream(fd,
                                                              path.c_str()));
    }
    Common::InSaveFile *openForLoading(const Common::String &name)
    {
        Common::String path;

        if (!resolveSavePath(name, path, false))
            return 0;

        int fd = rb->open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            setError(Common::kReadingFailed, "Could not open save file");
            return 0;
        }

        return new RockboxReadStream(fd, path.c_str());
    }
    Common::InSaveFile *openRawFile(const Common::String &name)
    {
        return openForLoading(name);
    }
    bool removeSavefile(const Common::String &name)
    {
        Common::String path;

        if (!resolveSavePath(name, path, false))
            return false;

        if (rb->remove(path.c_str()) == 0)
            return true;

        setError(Common::kPathDoesNotExist, "Could not remove save file");
        return false;
    }
    bool renameSavefile(const Common::String &oldName,
                        const Common::String &newName)
    {
        Common::String oldPath;
        Common::String newPath;

        if (!resolveSavePath(oldName, oldPath, false) ||
            !resolveSavePath(newName, newPath, true))
            return false;

        if (rb->rename(oldPath.c_str(), newPath.c_str()) == 0)
            return true;

        setError(Common::kWritingFailed, "Could not rename save file");
        return false;
    }
    bool copySavefile(const Common::String &oldName,
                      const Common::String &newName)
    {
        Common::String oldPath;
        Common::String newPath;
        char buffer[1024];

        if (!resolveSavePath(oldName, oldPath, false) ||
            !resolveSavePath(newName, newPath, true))
            return false;

        int in = rb->open(oldPath.c_str(), O_RDONLY);
        if (in < 0) {
            setError(Common::kReadingFailed, "Could not open source save");
            return false;
        }

        int out = rb->open(newPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                           0666);
        if (out < 0) {
            rb->close(in);
            setError(Common::kWritingFailed, "Could not create target save");
            return false;
        }

        bool ok = true;
        while (true) {
            ssize_t got = rb->read(in, buffer, sizeof(buffer));
            if (got < 0) {
                ok = false;
                setError(Common::kReadingFailed, "Could not read save file");
                break;
            }
            if (got == 0)
                break;
            if (rb->write(out, buffer, got) != got) {
                ok = false;
                setError(Common::kWritingFailed, "Could not write save file");
                break;
            }
        }

        rb->close(out);
        rb->close(in);
        if (!ok)
            rb->remove(newPath.c_str());
        return ok;
    }
    Common::StringArray listSavefiles(const Common::String &pattern)
    {
        Common::StringArray saves;
        Common::String dir = saveDir();
        DIR *d = rb->opendir(dir.c_str());
        struct dirent *entry;

        if (!d)
            return saves;

        while ((entry = rb->readdir(d))) {
            Common::String name(entry->d_name);
            if (name.empty() || name[0] == '.')
                continue;
            Common::String path = dir;
            appendPath(path, name);
            if (rb->dir_exists(path.c_str()))
                continue;
            if (Common::matchString(name.c_str(), pattern.c_str(), true))
                saves.push_back(name);
        }

        rb->closedir(d);
        return saves;
    }
    void updateSavefilesList(Common::StringArray &lockedFiles)
    {
        lockedFiles.clear();
    }

private:
    static void appendPath(Common::String &path, const Common::String &leaf)
    {
        if (!path.empty() && path.lastChar() != '/')
            path += '/';
        path += leaf;
    }

    static bool validSaveName(const Common::String &name)
    {
        return !name.empty() &&
               !rb->strchr(name.c_str(), '/') &&
               !rb->strchr(name.c_str(), '\\');
    }

    Common::String saveDir() const
    {
        Common::String dir = ConfMan.get("savepath");
        if (dir.empty())
            dir = "/.rockbox/scummvm/saves";
        return dir;
    }

    bool ensureDir(const Common::String &path)
    {
        char partial[MAX_PATH];
        size_t len = path.size();
        size_t i;

        if (len >= sizeof(partial)) {
            setError(Common::kPathDoesNotExist, "Save path is too long");
            return false;
        }

        rb->strlcpy(partial, path.c_str(), sizeof(partial));
        for (i = 1; partial[i] != '\0'; i++) {
            if (partial[i] != '/')
                continue;
            partial[i] = '\0';
            if (partial[0] != '\0' && !rb->dir_exists(partial) &&
                (rb->mkdir)(partial) < 0) {
                setError(Common::kPathDoesNotExist,
                         "Could not create save directory");
                return false;
            }
            partial[i] = '/';
        }

        if (!rb->dir_exists(partial) && (rb->mkdir)(partial) < 0) {
            setError(Common::kPathDoesNotExist,
                     "Could not create save directory");
            return false;
        }

        return true;
    }

    bool resolveSavePath(const Common::String &name, Common::String &path,
                         bool createDir)
    {
        clearError();

        if (!validSaveName(name)) {
            setError(Common::kPathDoesNotExist, "Invalid save file name");
            return false;
        }

        Common::String dir = saveDir();
        if (createDir && !ensureDir(dir))
            return false;

        path = dir;
        appendPath(path, name);
        return true;
    }
};

class RockboxOSystem : public OSystem {
public:
    RockboxOSystem() : _width(SCUMMVM_SURFACE_W), _height(SCUMMVM_SURFACE_H),
        _shakeOffset(0), _start(*rb->current_tick), _video(0)
    {
        _eventManager = new RockboxEventManager();
        _timerManager = new RockboxTimerManager();
        _savefileManager = new RockboxSaveFileManager();
        _fsFactory = new RockboxFilesystemFactory();
        _surface.create(SCUMMVM_SURFACE_W, SCUMMVM_SURFACE_H,
                        Graphics::PixelFormat::createFormatCLUT8());
    }

    ~RockboxOSystem()
    {
        _surface.free();
    }

    void setVideo(scummvm_video *video) { _video = video; }
    RockboxEventManager *events()
    {
        return static_cast<RockboxEventManager *>(_eventManager);
    }

    const GraphicsMode *getSupportedGraphicsModes() const
    {
        static const GraphicsMode modes[] = {
            { "normal", "Normal", 0 },
            { 0, 0, 0 }
        };
        return modes;
    }
    int getDefaultGraphicsMode() const { return 0; }
    bool setGraphicsMode(int mode) { return mode == 0; }
    int getGraphicsMode() const { return 0; }
    void initSize(uint width, uint height,
                  const Graphics::PixelFormat *format = NULL)
    {
        (void)format;
        _width = width;
        _height = height;
        _surface.free();
        _surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
    }
    int16 getHeight() { return _height; }
    int16 getWidth() { return _width; }
    PaletteManager *getPaletteManager() { return &_palette; }
    void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h)
    {
        const byte *src = static_cast<const byte *>(buf);
        byte *dst = static_cast<byte *>(_surface.getBasePtr(x, y));
        for (int row = 0; row < h; row++)
            rb->memcpy(dst + row * _surface.pitch, src + row * pitch, w);
    }
    Graphics::Surface *lockScreen() { return &_surface; }
    void unlockScreen() {}
    void fillScreen(uint32 col)
    {
        rb->memset(_surface.getPixels(), (int)col,
                   _surface.pitch * _surface.h);
    }
    void updateScreen()
    {
        if (!_video || !_surface.getPixels())
            return;
        int copyW = MIN((int)_width, _video->width);
        int copyH = MIN((int)_height, _video->height);
        const byte *src = static_cast<const byte *>(_surface.getPixels());
        for (int y = 0; y < copyH; y++) {
            fb_data *dst = _video->pixels + y * _video->width;
            for (int x = 0; x < copyW; x++)
                dst[x] = _palette.color(src[y * _surface.pitch + x]);
        }
        scummvm_video_present(_video);
        rb->lcd_update();
    }
    void setShakePos(int shakeOffset) { _shakeOffset = shakeOffset; }
    void showOverlay() {}
    void hideOverlay() {}
    Graphics::PixelFormat getOverlayFormat() const
    {
        return Graphics::PixelFormat::createFormatCLUT8();
    }
    void clearOverlay() {}
    void grabOverlay(void *buf, int pitch) { (void)buf; (void)pitch; }
    void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h)
    {
        (void)buf; (void)pitch; (void)x; (void)y; (void)w; (void)h;
    }
    int16 getOverlayHeight() { return _height; }
    int16 getOverlayWidth() { return _width; }
    bool showMouse(bool visible) { (void)visible; return true; }
    void warpMouse(int x, int y) { events()->setInput(x, y, false, false); }
    void setMouseCursor(const void *buf, uint w, uint h, int hotspotX,
                        int hotspotY, uint32 keycolor, bool dontScale = false,
                        const Graphics::PixelFormat *format = NULL)
    {
        (void)buf; (void)w; (void)h; (void)hotspotX; (void)hotspotY;
        (void)keycolor; (void)dontScale; (void)format;
    }
    uint32 getMillis(bool skipRecord = false)
    {
        (void)skipRecord;
        return (uint32)((*rb->current_tick - _start) * 1000 / HZ);
    }
    void delayMillis(uint msecs) { rb->sleep(msecs * HZ / 1000); }
    void getTimeAndDate(TimeDate &t) const { rb->memset(&t, 0, sizeof(t)); }
    MutexRef createMutex() { return reinterpret_cast<MutexRef>(this); }
    void lockMutex(MutexRef mutex) { (void)mutex; }
    void unlockMutex(MutexRef mutex) { (void)mutex; }
    void deleteMutex(MutexRef mutex) { (void)mutex; }
    Audio::Mixer *getMixer() { return &_mixer; }
    void quit()
    {
        Common::Event event;

        event.type = Common::EVENT_QUIT;
        events()->pushEvent(event);
    }
    void displayMessageOnOSD(const char *msg) { (void)msg; }
    void displayActivityIconOnOSD(const Graphics::Surface *icon)
    {
        (void)icon;
    }
    void logMessage(LogMessageType::Type type, const char *message)
    {
        (void)type; (void)message;
    }

private:
    int16 _width;
    int16 _height;
    int _shakeOffset;
    long _start;
    scummvm_video *_video;
    Graphics::Surface _surface;
    RockboxPaletteManager _palette;
    RockboxMixer _mixer;
};

static RockboxOSystem *queen_system;
static Queen::QueenEngine *queen_engine;

struct SkyIntroStage {
    uint16_t screen_file;
    uint16_t palette_file;
    uint16_t sequence_file;
    unsigned int frames;
};

static const SkyIntroStage sky_intro_stages[] = {
    { 60110, 60111, 0, 90 },
    { 60112, 60113, 0, 240 },
    { 60114, 60115, 0, 60 },
    { 60081, 60080, 60082, 0 },
    { 0, 0, 60083, 0 },
    { 0, 0, 60084, 0 },
    { 0, 0, 60085, 0 },
    { 0, 0, 60086, 0 },
};

class RockboxScummEngine {
public:
    RockboxScummEngine()
        : _target(0), _frame(0), _sky_intro_stage(0)
    {
        _sky_sequence_tick = 0;
        _sky_runtime_tick = 0;
        _sky_bootstrapped = false;
    }

    void setTarget(const scummvm_target *target)
    {
        _target = target;
        _frame = 0;
        _sky_intro_stage = 0;
        _sky_sequence_tick = 0;
        _sky_runtime_tick = 0;
        _sky_bootstrapped = false;
    }

    bool init(scummvm_engine_state *state)
    {
        if (!scumm_stricmp(_target->engine, "sky")) {
            if (!loadSkyIntroStage(state))
                return false;
            return true;
        }

        if (!scumm_stricmp(_target->engine, "queen")) {
            DEBUGF("scummvm: upstream queen init start path=%s\n",
                   _target->path);
            upstream_status_screen("queen init", _target->path);
            if (!queen_system) {
                upstream_status_screen("create osystem", 0);
                queen_system = new RockboxOSystem();
                g_system = queen_system;
            }
            DEBUGF("scummvm: upstream queen os ready\n");

            upstream_status_screen("configure engine", _target->gameid);
            ConfMan.addGameDomain(_target->gameid);
            ConfMan.setActiveDomain(_target->gameid);
            ConfMan.set("path", _target->path);
            ConfMan.set("savepath", _target->savepath);
            ConfMan.setBool("native_mt32", false);
            ConfMan.setBool("enable_gs", false);
            ConfMan.setBool("multi_midi", false);
            ConfMan.set("music_driver", "null");
            ConfMan.set("opl_driver", "null");
            ConfMan.setBool("aspect_ratio", false);
            ConfMan.setBool("fullscreen", false);
            ConfMan.setBool("enable_unsupported_game_warning", false);
            ConfMan.setBool("subtitles", true);
            ConfMan.setBool("alt_intro", false);
            ConfMan.setInt("autosave_period", 0);
            ConfMan.setInt("talkspeed", 60);
            ConfMan.setInt("save_slot", -1);
            ConfMan.setInt("music_volume", Audio::Mixer::kMaxMixerVolume);
            ConfMan.setInt("sfx_volume", Audio::Mixer::kMaxMixerVolume);
            ConfMan.setInt("speech_volume", Audio::Mixer::kMaxMixerVolume);
            ConfMan.setBool("mute", false);
            ConfMan.setBool("music_mute", true);
            ConfMan.setBool("sfx_mute", true);
            ConfMan.setBool("speech_mute", true);

            delete queen_engine;
            DEBUGF("scummvm: upstream queen create engine\n");
            upstream_status_screen("create queen", 0);
            queen_engine = new Queen::QueenEngine(queen_system);
            DEBUGF("scummvm: upstream queen initialize path\n");
            upstream_status_screen("initialize path", _target->path);
            queen_engine->initializePath(Common::FSNode(_target->path));
            DEBUGF("scummvm: upstream queen rockboxInit\n");
            upstream_status_screen("rockbox init", 0);
            if (queen_engine->rockboxInit().getCode() != Common::kNoError) {
                rb->strlcpy(state->status, "Queen upstream init failed",
                            sizeof(state->status));
                delete queen_engine;
                queen_engine = 0;
                return false;
            }
            upstream_status_screen("queen ready", 0);
            rb->strlcpy(state->status, "Queen upstream engine running",
                        sizeof(state->status));
            return true;
        }

        rb->snprintf(state->status, sizeof(state->status),
                     "C++ bridge ready for %s", _target->engine);
        return true;
    }

    bool drawFrame(scummvm_engine_state *state, scummvm_video *video)
    {
        int x;
        int y;

        if (_target && !scumm_stricmp(_target->engine, "sky")) {
            bool ok = _sky_bootstrapped ?
                scummvm_sky_runtime_render(video) :
                scummvm_sky_loader_render_current(video);
            advanceSkyIntro(state);
            return ok;
        }

        if (_target && !scumm_stricmp(_target->engine, "queen") &&
            queen_engine && queen_system) {
            queen_system->setVideo(video);
            queen_engine->rockboxStep();
            queen_system->updateScreen();
            rb->strlcpy(state->status, "Queen upstream frame",
                        sizeof(state->status));
            return true;
        }

        for (y = 0; y < video->height; y++) {
            for (x = 0; x < video->width; x++) {
                int r = (x + _frame) & 0xff;
                int g = (y * 2) & 0xff;
                int b = ((x / 8 + y / 8 + _frame / 8) & 1) ? 96 : 32;
                video->pixels[y * video->width + x] = LCD_RGBPACK(r, g, b);
            }
        }

        _frame++;
        return true;
    }

private:
    bool loadSkyIntroStage(scummvm_engine_state *state)
    {
        const SkyIntroStage *stage = &sky_intro_stages[_sky_intro_stage];

        if (stage->screen_file) {
            if (!scummvm_sky_loader_load_screen(_target, stage->screen_file,
                                                stage->palette_file,
                                                state->status,
                                                sizeof(state->status)))
                return false;
        }

        if (stage->sequence_file) {
            if (!scummvm_sky_loader_load_sequence(_target,
                                                  stage->sequence_file,
                                                  state->status,
                                                  sizeof(state->status)))
                return false;
        }

        rb->snprintf(state->status, sizeof(state->status),
                     "Sky intro stage %u/%u",
                     (unsigned)(_sky_intro_stage + 1),
                     (unsigned)ARRAYLEN(sky_intro_stages));
        return true;
    }

    void advanceSkyIntro(scummvm_engine_state *state)
    {
        char status[96];
        const SkyIntroStage *stage = &sky_intro_stages[_sky_intro_stage];

        if (stage->sequence_file) {
            _sky_sequence_tick++;
            if (_sky_sequence_tick >= 2) {
                _sky_sequence_tick = 0;
                scummvm_sky_loader_step_sequence();
            }

            if (scummvm_sky_loader_sequence_running())
                return;
        } else {
            _frame++;
            if (_frame < stage->frames)
                return;
        }

        if (_sky_intro_stage + 1 >= ARRAYLEN(sky_intro_stages)) {
            if (!_sky_bootstrapped &&
                scummvm_sky_loader_bootstrap_section0(_target,
                                                      state->status,
                                                      sizeof(state->status)) &&
                scummvm_sky_runtime_init(_target,
                                         state->status,
                                         sizeof(state->status)))
                _sky_bootstrapped = true;
            else if (_sky_bootstrapped) {
                _sky_runtime_tick++;
                if (_sky_runtime_tick >= 30) {
                    _sky_runtime_tick = 0;
                    scummvm_sky_runtime_step(state->status,
                                             sizeof(state->status));
                }
            }
            return;
        }

        _frame = 0;
        _sky_sequence_tick = 0;
        _sky_intro_stage++;
        if (!startSkyIntroStage(status, sizeof(status)))
            _sky_intro_stage--;
        else
            rb->strlcpy(state->status, status, sizeof(state->status));
    }

    bool startSkyIntroStage(char *status, size_t status_size)
    {
        scummvm_engine_state state;

        rb->memset(&state, 0, sizeof(state));
        if (!loadSkyIntroStage(&state)) {
            rb->strlcpy(status, state.status, status_size);
            return false;
        }

        rb->strlcpy(status, state.status, status_size);
        return true;
    }

    const scummvm_target *_target;
    unsigned int _frame;
    unsigned int _sky_intro_stage;
    unsigned int _sky_sequence_tick;
    unsigned int _sky_runtime_tick;
    bool _sky_bootstrapped;
};

static RockboxScummEngine active_engine;
static bool active_engine_ready;

bool scummvm_upstream_can_run(const struct scummvm_target *target)
{
    return !scumm_stricmp(target->engine, "sky") ||
           !scumm_stricmp(target->engine, "queen");
}

bool scummvm_upstream_init(const struct scummvm_target *target,
                           struct scummvm_engine_state *state)
{
    upstream_fatal_armed = true;
    if (setjmp(upstream_fatal_jmp) != 0) {
        upstream_fatal_armed = false;
        rb->strlcpy(state->status, upstream_fatal_message,
                    sizeof(state->status));
        scummvm_upstream_shutdown();
        return false;
    }
    active_engine.setTarget(target);
    active_engine_ready = active_engine.init(state);
    upstream_fatal_armed = false;
    return active_engine_ready;
}

void scummvm_upstream_input(int x, int y, bool down, bool clicked)
{
    if (queen_system)
        queen_system->events()->setInput(x, y, down, clicked);
}

bool scummvm_upstream_frame(const struct scummvm_target *target,
                            struct scummvm_engine_state *state,
                            struct scummvm_video *video)
{
    (void)target;

    if (!active_engine_ready)
        return false;

    upstream_fatal_armed = true;
    if (setjmp(upstream_fatal_jmp) != 0) {
        upstream_fatal_armed = false;
        rb->strlcpy(state->status, upstream_fatal_message,
                    sizeof(state->status));
        scummvm_upstream_shutdown();
        return false;
    }

    bool ok = active_engine.drawFrame(state, video);
    upstream_fatal_armed = false;
    return ok;
}

void scummvm_upstream_shutdown(void)
{
    if (!active_engine_ready && !queen_engine && !queen_system)
        return;

    DEBUGF("scummvm: upstream shutdown begin\n");
    active_engine_ready = false;
    trace_stream_dump_open();
    delete queen_engine;
    queen_engine = 0;
    DEBUGF("scummvm: upstream engine deleted\n");
    trace_stream_dump_open();
    if (queen_system) {
        SearchMan.clear();
        DEBUGF("scummvm: upstream search cleared\n");
        trace_stream_dump_open();
    }
    delete queen_system;
    if (g_system == queen_system)
        g_system = 0;
    queen_system = 0;
    DEBUGF("scummvm: upstream shutdown end\n");
}
