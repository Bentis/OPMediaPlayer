#include <jni.h>

#include <android/asset_manager_jni.h>

#include "logging.h"
#include "OPMediaPlayer.h"


extern "C" {

std::unique_ptr<OPMediaPlayer> player = nullptr;

JNIEXPORT void JNICALL
Java_net_bentis_opmediaplayer_OPMediaPlayer_native_1onCreate(JNIEnv *env, jobject instance) {

    LOGD("onCreate");
    if (player == nullptr) {
        player = std::make_unique<OPMediaPlayer>(new OPMediaPlayer());
    }
}

JNIEXPORT void JNICALL
Java_net_bentis_opmediaplayer_OPMediaPlayer_native_1onHandleIntent(JNIEnv *env, jobject instance, jstring jfilename)
{
    const char * filename = env->GetStringUTFChars(jfilename, JNI_FALSE);
    LOGD("filename: %s", filename);
    if (filename != nullptr)
    {
        player->playFile(filename);
        env->ReleaseStringUTFChars(jfilename, filename);
    }
}

}