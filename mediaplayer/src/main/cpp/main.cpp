#include <iostream>
#include <chrono>
#include <thread>
#include "OPMediaPlayer.h"

int main(int argc, const char * argv[])
{
    if (argc > 1) {
        auto player = new OPMediaPlayer();
        player->playFile(argv[1]);

        // Play finished?
        while (player->isPlaying()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    else {
        std::cerr << "Missing filename" << std::endl;
        return 1;
    }
    return 0;
}
