#pragma once
#include "miniaudio.h"
#include <cstdio>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

/**
 * @brief This class is a wrapper around ma_decoder for safer use
 *
 */
class Decoder {
public:
  Decoder(nlohmann::json &data) {
    std::string filePath = data["path"];
    volume = data["volume"];
    ma_result result = ma_decoder_init_file(filePath.c_str(), NULL, &m_decoder);
    printf("test");
    if (result != MA_SUCCESS) {
      // return -2;
      printf("ERROR!!!");
      Pause();
      return;
    }
    ready = true;
  }

  ~Decoder() { ma_decoder_uninit(&m_decoder); }

  float volume{1.0f};

private:
  ma_decoder m_decoder{};
  ma_bool32 m_playing{true};
  ma_bool32 ready{false};

public:
  // seeking
  void Seek(ma_uint64 pos) {
    if (!ready) { // not ready
      return;
    }
    ma_decoder_seek_to_pcm_frame(&m_decoder, pos);
  }

  // get progress in pcm frames
  ma_uint64 GetPos() {
    if (!ready) { // not ready
      return 0;
    }
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
    if (!ready) { // not ready
      return 0;
    }

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

/**
 * @brief This class combines on or multiple Decoders to one output
 * stream.
 *
 */
class OutputStream {
public:
  /**
   * @brief the config json is expected as array
   *
   */
  OutputStream(nlohmann::json &config /*expected as array*/) {
    if (!config.is_array()) {
      return;
    }

    m_decoderCount = config.size(); // check if array is empty
    if (m_decoderCount == 0) {
      return;
    }

    if (m_decoderCount > 1) { // TODO add multiple decoder combining
      printf("Multiple inputs on one output not supported jet, using first "
             "one"); // TODO add multiple decoder combining
      m_decoderCount = 1;
    }

    m_decoder = (Decoder *)malloc(
        sizeof(Decoder) * m_decoderCount); // allocate and create decoders

    for (ma_uint32 i; i < m_decoderCount; i++) {
      m_decoder[i] = Decoder(config[i]);
    }
  };
  ~OutputStream() {
    for (ma_uint32 i = 0; i < m_decoderCount; i++) {
      m_decoder[i].~Decoder();
    }
    delete m_decoder;
  }

  /**
   * @brief read pcm frames from all inputs
   *
   * @param dst output buffer
   * @param count requested pcm frame count
   */
  void ReadPCMFrames(void *dst, ma_uint64 count) {
    if (!ready) {
      return;
    }

    // TODO add multiple decoder combining
    m_decoder[0].ReadPCMFrames(dst, count);
  };

  // start
  void Start(bool fromBeginning = false) {
    for (ma_uint32 i = 0; i < m_decoderCount; i++) {
      m_decoder[i].Start(fromBeginning);
    }
  }
  // pause
  void Pause() {
    for (ma_uint32 i = 0; i < m_decoderCount; i++) {
      m_decoder[i].Pause();
    }
  }
  void Seek(ma_uint64 pos) {
    for (ma_uint32 i = 0; i < m_decoderCount; i++) {
      m_decoder[i].Seek(pos);
    }
  }

private:
  bool ready{false};
  Decoder *m_decoder{nullptr};
  ma_uint32 m_decoderCount{0};
};