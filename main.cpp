#include "Decoder.h"
#include "json.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <vector>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

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

    nlohmann::json &tracks = config["tracks"];
    if (!tracks.is_array()) { // files missing
      printf("ERROR files missing\n");
      return;
    }

    decoder.clear();
    decoder.resize(tracks.size());
    int i = 0;
    for (nlohmann::json &f : tracks) {
      decoder.push_back(std::make_shared<Track>(f));
      if (!decoder.back()->Ready()) {
        printf("Warning file setup failed/not routed, removing\n");
        decoder.pop_back();
      }
      i++;
    }
    decoder.shrink_to_fit();
  }

  std::vector<std::shared_ptr<Track>> decoder;
};

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
  /* uint offset = 0;
   Data *data = (Data *)pDevice->pUserData;
   for (int i = 0; i < 4; i++) {

     if (data->decoder[i] == NULL) {
       return;
     }

     ma_decoder_read_pcm_frames(data->decoder[i], pOutput, frameCount, NULL);
   }
   (void)pInput;*/
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

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = ma_format_f32;
  deviceConfig.playback.channels = OUT_CHANNEL_COUNT;
  deviceConfig.sampleRate = SAMPLE_RATE;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &data;

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