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
#include <fstream>
#include "logging.h"

#include "PlayAudioEngine.h"

constexpr int32_t kDefaultChannelCount = 2; // Stereo

PlayAudioEngine::PlayAudioEngine()
{
    mChannelCount = kDefaultChannelCount;
    createPlaybackStream();
}

PlayAudioEngine::~PlayAudioEngine()
{
    closeOutputStream();
    LOGD("Closing play engine");
}

bool PlayAudioEngine::isPlaying() const
{
    return mData != nullptr;
}

/**
 * Creates an audio stream for playback. The audio device used will depend on mPlaybackDeviceId.
 */
void PlayAudioEngine::createPlaybackStream()
{
    oboe::AudioStreamBuilder builder;
    setupPlaybackStreamParameters(&builder);

    oboe::Result result = builder.openStream(&mPlayStream);

    if (result == oboe::Result::OK && mPlayStream != nullptr) {

        mFramesPerBurst = mPlayStream->getFramesPerBurst();

        // Set the buffer size to the burst size - this will give us the minimum possible latency
        mPlayStream->setBufferSizeInFrames(mFramesPerBurst);

        // Start the stream - the dataCallback function will start being called
        result = mPlayStream->requestStart();
        if (result != oboe::Result::OK) {
            LOGE("Error starting stream. %s", oboe::convertToText(result));
        }
    } else {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
    }
}

void PlayAudioEngine::playFile(const char * filename)
{
    std::lock_guard<std::mutex> lock(mDataLock);

    if (mData != nullptr)  {
        delete mData;
        mData = nullptr;
    }

    std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
    LOGD("file state: %i, fail: %i, bad: %i", file.good(), file.fail(), file.bad());
    if (file.is_open()) {
        // Parse header
        int32_t length = 0;
        if (!parseWave(file, &length)) {
            LOGE("Failed to parse WAVE file.");

            // Fallback?
            file.seekg (0, file.end);
            length = file.tellg();
            file.seekg (0, file.beg);
        }

        int samples = (length)/2;
        int16_t * data = new int16_t[samples];
        int index = 0;
        char buffer[2];
        while(!file.eof() && index < samples) {
            file.read(buffer, 2);
            data[index++] = *reinterpret_cast<int16_t*>(buffer);
        }

        // There are 4 bytes per frame because
        // each sample is 2 bytes and
        // it's a stereo recording which has 2 samples per frame.
        mTotalFrames = (int32_t) (samples/2);
        mReadFrameIndex = 0;

        mData = data;
        LOGD("length: %i, samples: %i, mTotalFrames: %i, index: %i", length, samples, mTotalFrames, index);

        file.close();
    } else {
        LOGE("could not open file: %s", filename);
    }
}

bool PlayAudioEngine::parseWave(std::ifstream &file, int32_t *length)
{
    char buffer[4];
    file.read(buffer, 4);
    if (strncmp(buffer, "RIFF", 4) != 0)
        return false;

    file.seekg(8);
    file.read(buffer, 4);
    if (strncmp(buffer, "WAVE", 4) != 0)
        return false;

    // Find data segment
    int chuckPos = 12;
    while (file.good()) {
        file.read(buffer, 4);
        // TODO: Verify fmt chunk? should be PCM,16bit,2ch,44100kHz
        if (strncmp(buffer, "data", 4) == 0) {
            // FOUND IT!
            file.read(buffer, 4);
            *length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
            return true;
        }
        else {
            // different chunk
            file.read(buffer, 4);
            int32_t size = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
            chuckPos += 8 + size;
            file.seekg(chuckPos);
        }
    }
    return false;
}


/**
 * Sets the stream parameters which are specific to playback, including device id and the
 * callback class, which must be set for low latency playback.
 * @param builder The playback stream builder
 */
void PlayAudioEngine::setupPlaybackStreamParameters(oboe::AudioStreamBuilder *builder)
{
    builder->setAudioApi(mAudioApi);
    builder->setDeviceId(mPlaybackDeviceId);
    builder->setChannelCount(mChannelCount);
    builder->setDirection(oboe::Direction::Output);
    builder->setUsage(oboe::Usage::NotificationEvent);

    // We request EXCLUSIVE mode since this will give us the lowest possible latency.
    // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
    builder->setSharingMode(oboe::SharingMode::Exclusive);
    builder->setPerformanceMode(oboe::PerformanceMode::LowLatency);

    // Assume I16, 2ch, 44.1kHZ
    builder->setFormat(oboe::AudioFormat::I16);
    builder->setChannelCount(2);
    builder->setSampleRate(44100);

    builder->setCallback(this);
}

void PlayAudioEngine::closeOutputStream()
{
    if (mPlayStream != nullptr) {
        oboe::Result result = mPlayStream->requestStop();
        if (result != oboe::Result::OK) {
            LOGE("Error stopping output stream. %s", oboe::convertToText(result));
        }

        result = mPlayStream->close();
        if (result != oboe::Result::OK) {
            LOGE("Error closing output stream. %s", oboe::convertToText(result));
        }
    }
}

/**
 * Every time the playback stream requires data this method will be called.
 *
 * @param audioStream the audio stream which is requesting data, this is the mPlayStream object
 * @param audioData an empty buffer into which we can write our audio data
 * @param numFrames the number of audio frames which are required
 * @return Either oboe::DataCallbackResult::Continue if the stream should continue requesting data
 * or oboe::DataCallbackResult::Stop if the stream should stop.
 */
oboe::DataCallbackResult PlayAudioEngine::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames)
{
    bool isDone = false;

    if (mDataLock.try_lock()) {
        if (mData != nullptr) {
            int32_t soundFrames = numFrames;
            // Check whether we're about to reach the end of the recording
            if (mReadFrameIndex + numFrames >= mTotalFrames) {
                soundFrames = mTotalFrames - mReadFrameIndex;
                isDone = true;
            }

            for (int i = 0; i < soundFrames; ++i) {
                for (int j = 0; j < mChannelCount; ++j) {
                    static_cast<int16_t *>(audioData)[(i * mChannelCount) + j] = mData[(mReadFrameIndex * mChannelCount) + j];
                }

                // Increment and handle wraparound
                if (++mReadFrameIndex >= mTotalFrames) mReadFrameIndex = 0;
            }

            numFrames -= soundFrames;

            if (isDone) {
                delete mData;
                mData = nullptr;
                LOGD("Done playing");
            }
        }
        mDataLock.unlock();
    }

    // fill remaining with zeros to output silence
    if (numFrames > 0) {
        memset(static_cast<uint8_t *>(audioData), 0, sizeof(int16_t) * mChannelCount * numFrames);
    }

    return oboe::DataCallbackResult::Continue;
}

/**
 * If there is an error with a stream this function will be called. A common example of an error
 * is when an audio device (such as headphones) is disconnected. It is safe to restart the stream
 * in this method. There is no need to create a new thread.
 *
 * @param audioStream the stream with the error
 * @param error the error which occured, a human readable string can be obtained using
 * oboe::convertToText(error);
 *
 * @see oboe::StreamCallback
 */
void PlayAudioEngine::onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error)
{
    if (error == oboe::Result::ErrorDisconnected) restartStream();
}

void PlayAudioEngine::restartStream()
{
    LOGI("Restarting stream");

    if (mRestartingLock.try_lock()) {
        closeOutputStream();
        createPlaybackStream();
        mRestartingLock.unlock();
    } else {
        LOGW("Restart stream operation already in progress - ignoring this request");
        // We were unable to obtain the restarting lock which means the restart operation is currently
        // active. This is probably because we received successive "stream disconnected" events.
        // Internal issue b/63087953
    }
}