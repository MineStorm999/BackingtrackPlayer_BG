#include <cstdlib>
#include <unistd.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

void ExitOnFail(ma_result result){
  if (result != MA_SUCCESS) {
    printf("Error: %u", (uint)result);
    exit(-1);
  }
}

// Source - https://stackoverflow.com/a/28827188
// Posted by Bernardo Ramos, modified by community. See post 'Timeline' for change history
// Retrieved 2026-11-07, License - CC BY-SA 4.0

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sleep_ms(int milliseconds){ // cross-platform sleep function
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

      //ma_sound_group_set_fade_in_milliseconds(&sound, 0,1, 20);
      ma_sound_set_volume(&sound, 1);
      ExitOnFail(ma_sound_start(&sound));
      ma_sound_set_volume(&sound, 1);
      printf("Press Enter to pause");
    } else {
      ma_sound_group_set_fade_in_milliseconds(&sound, 1,0, 200);
      sleep_ms(200);
      ma_sound_stop(&sound);

      //ExitOnFail(ma_sound_stop_with_fade_in_milliseconds(&sound, 5));
      ExitOnFail(ma_sound_seek_to_pcm_frame(&sound, 0));
      printf("Press Enter to play");
    }

    getchar();
  }
  ma_engine_uninit(&engine);

  return 0;
}