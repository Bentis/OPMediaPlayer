#include "OPMediaPlayer.h"

OPMediaPlayer::OPMediaPlayer()
: mEngine(new PlayAudioEngine())
{
}

OPMediaPlayer::OPMediaPlayer(OPMediaPlayer * other)
{
    mEngine = other->mEngine;
}

void OPMediaPlayer::playFile(const char * filename)
{
    mEngine->playFile(filename);
}

bool OPMediaPlayer::isPlaying() const
{
    return mEngine->isPlaying();
}