/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <oboe/Oboe.h>

class PlayAudioEngine : oboe::AudioStreamCallback {

public:
    PlayAudioEngine();
    ~PlayAudioEngine();

    // oboe::StreamCallback methods
    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

    void playFile(const char * filename);

    bool isPlaying() const;

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error);

private:
    oboe::AudioApi mAudioApi = oboe::AudioApi::Unspecified;
    int32_t mPlaybackDeviceId = oboe::kUnspecified;

    oboe::AudioStream *mPlayStream;
    std::mutex mRestartingLock;

    std::mutex mDataLock;

    int32_t mChannelCount;
    int32_t mFramesPerBurst;

    // Audio file params:
    int32_t mReadFrameIndex = 0;
    const int16_t* mData = nullptr;
    int32_t mTotalFrames = 0;

    void createPlaybackStream();

    void closeOutputStream();

    void restartStream();

    void setupPlaybackStreamParameters(oboe::AudioStreamBuilder *builder);
    bool parseWave(std::ifstream &file, int32_t *length);
};
