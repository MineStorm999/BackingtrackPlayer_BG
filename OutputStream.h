#pragma once
#include "miniaudio.h"
/**
 * @brief This class is a wrapper around ma_decoder for safer use
 *
 */
class Decoder {

private:
  ma_decoder m_decoder;
  ma_bool32 m_playing;
  ma_uint64 m_pos; // in samples

public:
  // seeking
  void Seek(ma_uint64 pos) { ma_decoder_seek_to_pcm_frame(&m_decoder, pos); }

  // get progress in pcm frames
  ma_uint64 GetPos() {
    ma_uint64 ret = 0;
    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &ret);
    return ret;
  }

  // is playing?
  bool Playing() { return m_playing; }

  // pause playback
  void Pause() { m_playing = false; }

  // start playback
  void Start(bool fromBeginning = false) {
    if (fromBeginning) {
      Seek(0);
    }
    m_playing = true;
  }

  // read the pcm frames
  ma_uint64 ReadPCMFrames(void *dst, ma_uint64 count) {

    if (!m_playing) { // return early, if paused
      return 0;
    }

    ma_uint64 read = 0;
    ma_decoder_read_pcm_frames(&m_decoder, dst, count, &read); // read frames

    if (read < count) { // auto pause if requesting over range
      m_playing = false;
    }
    return read;
  }
};