#include "Track.h"
#include "json.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <vector>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <Eigen/Dense>

void ExitOnFail(ma_result result) {
  if (result != MA_SUCCESS) {
    printf("Error: %u", (uint)result);
    exit(-1);
  }
}

// Source - https://stackoverflow.com/a/28827188
// Posted by Bernardo Ramos, modified by community. See post 'Timeline' for
// change history Retrieved 2026-11-07, License - CC BY-SA 4.0

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h> // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds) { // cross-platform sleep function
#ifdef WIN32
  Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
#else
  if (milliseconds >= 1000)
    sleep(milliseconds / 1000);
  usleep((milliseconds % 1000) * 1000);
#endif
}

struct File {
  File(nlohmann::json &config) { FromFile(config); }

  void FromFile(nlohmann::json &config) {
    if (config.is_null()) { // config missing
      printf("ERROR config missing\n");
      return;
    }

    nlohmann::json &tracksJS = config["tracks"];
    if (!tracksJS.is_array()) { // files missing
      printf("ERROR files missing\n");
      return;
    }

    tracks.clear();
    tracks.reserve(tracksJS.size() * 100); // Testing
    // int i = 0;
    for (nlohmann::json &f : tracksJS) {
      for (int i = 0; i < 250; i++) {
        tracks.push_back(std::make_shared<Track>(f));
        if (!tracks.back()->Ready()) {
          printf("Warning file setup failed/not routed, removing\n");
          tracks.pop_back();
        }
      }
    }
    tracks.shrink_to_fit();
  }

  std::vector<std::shared_ptr<Track>> tracks;
  void Play() {
    for (auto t : tracks) {
      t->Play();
    }
  }
};

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
  if (!pDevice->pUserData) {
    return;
  }
  File *file = (File *)pDevice->pUserData;
  if (!file) {
    return;
  }
  Eigen::Map<Eigen::MatrixXf> dstMat((float *)pOutput, OUT_CHANNEL_COUNT,
                                     frameCount);
  for (auto track : file->tracks) {
    if (!track) {
      continue;
    }

    track->ReadPCMFramesAndMixToOutput(dstMat, frameCount);
  }
}

void Test() {
  printf("Test\n");
  std::ifstream f("config.json");
  nlohmann::json data = nlohmann::json::parse(f);
  Track dec(data[0]);
}

int main() {
  ma_result result;
  ma_decoder decoder;
  ma_device_config deviceConfig;
  ma_device device;
  std::ifstream f("config.json");
  nlohmann::json data = nlohmann::json::parse(f);

  File file(data);
  file.Play();

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = ma_format_f32;
  deviceConfig.playback.channels = OUT_CHANNEL_COUNT;
  deviceConfig.sampleRate = SAMPLE_RATE;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &file;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    ma_decoder_uninit(&decoder);
    return -3;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
    return -4;
  }

  printf("Press Enter to quit...");
  getchar();

  ma_device_uninit(&device);

  return 0;
}