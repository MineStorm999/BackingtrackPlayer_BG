#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

int main() {
  ma_result result;
  ma_engine engine;

  result = ma_engine_init(NULL, &engine);
  if (result != MA_SUCCESS) {
    return -1;
  }
  ma_sound sound;

  result = ma_sound_init_from_file(&engine, "SampleMedia/Braam 004 (D).wav", 0,
                                   NULL, NULL, &sound);
  if (result != MA_SUCCESS) {
    return result;
  }

  while (true) {

    if (!ma_sound_is_playing(&sound)) {
      ma_sound_set_volume(&sound, 1.0f);
      ma_sound_start(&sound);
      ma_sound_set_volume(&sound, 1.0f);
      printf("Press Enter to pause");
    } else {
      // ma_sound_stop(&sound);
      ma_sound_stop_with_fade_in_milliseconds(&sound, 5);
      printf("Press Enter to play");
    }

    getchar();
  }
  ma_engine_uninit(&engine);

  return 0;
}