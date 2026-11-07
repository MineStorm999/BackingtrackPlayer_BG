#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
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

struct Data {
  ma_decoder *decoder[4];
};

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
  uint offset = 0;
  Data *data = (Data *)pDevice->pUserData;
  for (int i = 0; i < 4; i++) {

    if (data->decoder[i] == NULL) {
      return;
    }

    ma_decoder_read_pcm_frames(data->decoder[i], pOutput, frameCount, NULL);
  }
  (void)pInput;
}

int main() {
  std::cout << "Test" << std::endl;
  printf("test");
  ma_result result;
  ma_decoder decoder;
  ma_device_config deviceConfig;
  ma_device device;

  /*if (argc < 2) {
      printf("No input file.\n");
      //return -1;
  }*/

  result = ma_decoder_init_file(
      "/home/spacedviking/Music/Campaign 1： Vox Machina/test.wav", NULL,
      &decoder);
  printf("test");
  if (result != MA_SUCCESS) {
    // return -2;
  }

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = decoder.outputFormat;
  deviceConfig.playback.channels = decoder.outputChannels * 4;
  deviceConfig.sampleRate = decoder.outputSampleRate;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &decoder;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    ma_decoder_uninit(&decoder);
    return -3;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
    return -4;
  }

  printf("Press Enter to quit...");
  getchar();

  ma_device_uninit(&device);
  ma_decoder_uninit(&decoder);

  return 0;
}