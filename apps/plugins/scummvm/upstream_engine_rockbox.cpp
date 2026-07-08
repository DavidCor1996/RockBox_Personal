/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_ /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "engines/engine.h"

#include "audio/mixer.h"
#include "common/config-manager.h"
#include "common/error.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "graphics/cursorman.h"

Engine *g_engine = 0;

ChainedGamesManager::ChainedGamesManager()
{
    clear();
}

void ChainedGamesManager::clear()
{
    _chainedGames.clear();
}

void ChainedGamesManager::push(const Common::String target, const int slot)
{
    Game game;
    game.target = target;
    game.slot = slot;
    _chainedGames.push(game);
}

bool ChainedGamesManager::pop(Common::String &target, int &slot)
{
    if (_chainedGames.empty())
        return false;

    Game game = _chainedGames.pop();
    target = game.target;
    slot = game.slot;
    return true;
}

namespace Common {
DECLARE_SINGLETON(ChainedGamesManager);
}

Engine::Engine(OSystem *syst)
    : _system(syst), _mixer(_system->getMixer()),
      _timer(_system->getTimerManager()),
      _eventMan(_system->getEventManager()),
      _saveFileMan(_system->getSavefileManager()),
      _mainMenuDialog(0),
      _targetName(ConfMan.getActiveDomainName()),
      _pauseLevel(0), _pauseStartTime(0),
      _engineStartTime(_system->getMillis()),
      _saveSlotToLoad(-1)
{
    g_engine = this;
    CursorMan.pushCursor(0, 0, 0, 0, 0, 0);
    CursorMan.pushCursorPalette(0, 0, 0);
}

Engine::~Engine()
{
    if (_mixer)
        _mixer->stopAll();

    CursorMan.popCursor();
    CursorMan.popCursorPalette();
    g_engine = 0;
}

void Engine::initializePath(const Common::FSNode &gamePath)
{
    SearchMan.addDirectory(gamePath.getPath(), gamePath, 0, 4);
}

void Engine::errorString(const char *buf_input, char *buf_output,
                         int buf_output_size)
{
    Common::strlcpy(buf_output, buf_input, buf_output_size);
}

void Engine::syncSoundSettings()
{
    bool mute = ConfMan.hasKey("mute") && ConfMan.getBool("mute");
    int music = ConfMan.hasKey("music_volume") ?
        ConfMan.getInt("music_volume") : Audio::Mixer::kMaxMixerVolume;
    int sfx = ConfMan.hasKey("sfx_volume") ?
        ConfMan.getInt("sfx_volume") : Audio::Mixer::kMaxMixerVolume;
    int speech = ConfMan.hasKey("speech_volume") ?
        ConfMan.getInt("speech_volume") : Audio::Mixer::kMaxMixerVolume;

    if (!_mixer)
        return;

    _mixer->muteSoundType(Audio::Mixer::kPlainSoundType, mute);
    _mixer->muteSoundType(Audio::Mixer::kMusicSoundType, mute);
    _mixer->muteSoundType(Audio::Mixer::kSFXSoundType, mute);
    _mixer->muteSoundType(Audio::Mixer::kSpeechSoundType, mute);
    _mixer->setVolumeForSoundType(Audio::Mixer::kPlainSoundType,
                                  Audio::Mixer::kMaxMixerVolume);
    _mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, music);
    _mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, sfx);
    _mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, speech);
}

void Engine::deinitKeymap()
{
}

void Engine::flipMute()
{
    bool mute = !(ConfMan.hasKey("mute") && ConfMan.getBool("mute"));
    ConfMan.setBool("mute", mute);
    syncSoundSettings();
}

Common::Error Engine::loadGameState(int slot)
{
    (void)slot;
    return Common::kNoError;
}

void Engine::setGameToLoadSlot(int slot)
{
    _saveSlotToLoad = slot;
}

bool Engine::canLoadGameStateCurrently()
{
    return false;
}

Common::Error Engine::saveGameState(int slot, const Common::String &desc)
{
    (void)slot;
    (void)desc;
    return Common::kNoError;
}

bool Engine::canSaveGameStateCurrently()
{
    return false;
}

void Engine::pauseEngineIntern(bool pause)
{
    if (_mixer)
        _mixer->pauseAll(pause);
}

void Engine::quitGame()
{
    Common::Event event;
    event.type = Common::EVENT_QUIT;
    g_system->getEventManager()->pushEvent(event);
}

bool Engine::shouldQuit()
{
    Common::EventManager *eventMan = g_system->getEventManager();
    return eventMan->shouldQuit() || eventMan->shouldRTL();
}

void Engine::pauseEngine(bool pause)
{
    if (pause) {
        _pauseLevel++;
        if (_pauseLevel == 1) {
            _pauseStartTime = _system->getMillis();
            pauseEngineIntern(true);
        }
    } else if (_pauseLevel > 0) {
        _pauseLevel--;
        if (_pauseLevel == 0) {
            pauseEngineIntern(false);
            _engineStartTime += _system->getMillis() - _pauseStartTime;
            _pauseStartTime = 0;
        }
    }
}

void Engine::openMainMenuDialog()
{
}

bool Engine::warnUserAboutUnsupportedGame()
{
    return true;
}

uint32 Engine::getTotalPlayTime() const
{
    if (_pauseLevel)
        return _pauseStartTime - _engineStartTime;
    return _system->getMillis() - _engineStartTime;
}

void Engine::setTotalPlayTime(uint32 time)
{
    uint32 currentTime = _system->getMillis();
    if (_pauseLevel > 0)
        _pauseStartTime = currentTime;
    _engineStartTime = currentTime - time;
}

void Engine::checkCD()
{
}

bool Engine::shouldPerformAutoSave(int lastSaveTime)
{
    (void)lastSaveTime;
    return false;
}

int Engine::runDialog(GUI::Dialog &dialog)
{
    (void)dialog;
    return 0;
}

void initGraphics(int width, int height, bool defaultTo1xScaler,
                  const Graphics::PixelFormat *format)
{
    (void)defaultTo1xScaler;
    g_system->beginGFXTransaction();
#ifdef USE_RGB_COLOR
    g_system->initSize(width, height, format);
#else
    (void)format;
    g_system->initSize(width, height);
#endif
    g_system->endGFXTransaction();
}

void initGraphics(int width, int height, bool defaultTo1xScaler,
                  const Common::List<Graphics::PixelFormat> &formatList)
{
    Graphics::PixelFormat format = formatList.empty() ?
        Graphics::PixelFormat::createFormatCLUT8() : formatList.front();
    initGraphics(width, height, defaultTo1xScaler, &format);
}

void initGraphics(int width, int height, bool defaultTo1xScaler)
{
    Graphics::PixelFormat format = Graphics::PixelFormat::createFormatCLUT8();
    initGraphics(width, height, defaultTo1xScaler, &format);
}

void GUIErrorMessage(const Common::String &msg)
{
    error("%s", msg.c_str());
}
