#pragma once

#include "PlayAudioEngine.h"

class OPMediaPlayer {
public:
    OPMediaPlayer();
    OPMediaPlayer(OPMediaPlayer * other);
    void playFile(const char * filename);
    bool isPlaying() const;
private:
    PlayAudioEngine * mEngine;
};