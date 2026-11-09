#pragma once
#include "defines.h"
#include "miniaudio.h"
#include "thirdparty/json.hpp"
#include <cstdio>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#define TEMP_BUFFER_SIZE 4096

/**
 * @brief This class is a wrapper around ma_decoder for safer use
 *
 */
class Track {
public:
  Track(nlohmann::json &data) {
    if (data.is_null()) {
      printf("ERROR data is null\n");
      return;
    }

    if (!data["path"].is_string()) {
      printf("ERROR path is empty\n");
      return;
    }
    std::string path = data["path"]; // retrieve json data

    if (!data["volume"].is_number()) {
      printf("ERROR volume is empty, defaulting to 1.0f\n");
      m_volume = 1.0f;
    } else {
      m_volume = data["volume"];
    }

    nlohmann::json routing = (data["routing"]); // reading -
    if (!routing.is_array()) { //  - and verifying routing matrix from json
      printf("ERROR routing matrix is in wrong syntax/missing\n");
      // std::cout << routing.dump(4);
      return;
    }

    auto conf = ma_decoder_config_init(ma_format_f32, 0,
                                       SAMPLE_RATE); // init internal decoder
    ma_result r = ma_decoder_init_file(path.c_str(), &conf, &m_decoder);
    if (r != MA_SUCCESS) {
      printf("ERROR internal decoder init failed\n");
      return;
    }

    if (routing.size() < GetChannelCount()) {
      printf("ERROR routing matrix is missing channels\n");
      // std::cout << routing.dump(4);
      ma_decoder_uninit(&m_decoder);
      return;
    }
    printf("Routed Channels: %u\n", (ma_uint32)routing.size());

    m_routingMatrix.resize(GetChannelCount());

    for (int i = 0; i < GetChannelCount(); i++) {
      nlohmann::json &jDstChannels =
          routing[i]; // retrieving per channel route dsts from
                      // json and verifying it
      if (!jDstChannels.is_null()) {
        printf("ERROR routing matrix is missing (channel %i)\n", i);
        // std::cout << jDstChannels.dump(4);
        ma_decoder_uninit(&m_decoder);
        return;
      }

      m_routingMatrix[i].resize(jDstChannels.size());
      for (int j = 0; j < m_routingMatrix[i].size(); j++) {
        m_routingMatrix[i][j] = jDstChannels[j];

        if (jDstChannels[j] >=
            OUT_CHANNEL_COUNT) { // dst channel id out of range
          printf(
              "ERROR routing matrix is in wrong: st channel id out of range\n");
          ma_decoder_uninit(&m_decoder);
          return;
        }
      }
      m_routingMatrix[i].shrink_to_fit();
    }
    m_routingMatrix.shrink_to_fit();
    printf("Successfully created Decoder for %s\n", path.c_str());
  }

  ~Track() { _Uninit(); };

private:
  ma_decoder m_decoder;
  ma_bool32 m_playing{false};
  ma_bool32 m_ready{false};
  ma_uint64 m_pos{0}; // in samples

  float m_volume{1.0f};
  std::vector<std::vector<ma_uint32>> m_routingMatrix;

  float m_tempBuffer[TEMP_BUFFER_SIZE]; // to store read data

  void _Uninit() {
    if (m_ready) {
      ma_decoder_uninit(&m_decoder);
    }
    m_ready = false;
    m_routingMatrix.clear();
  }

public:
  bool Ready() { return m_ready; }

  ma_uint32 GetChannelCount() {
    if (!m_ready) {
      return 0;
    }
    return m_decoder.outputChannels;
  }

  // seeking
  void Seek(ma_uint64 pos) {
    if (!m_ready) {
      return;
    }
    ma_decoder_seek_to_pcm_frame(&m_decoder, pos);
  }

  // get progress in pcm frames
  ma_uint64 GetPos() {
    if (!m_ready) {
      return 0;
    }
    ma_uint64 ret = 0;
    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &ret);
    return ret;
  }

  // is playing?
  bool Playing() { return m_playing; }

  bool NeedsProcessing() { return m_playing && m_ready; }

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
  ma_uint64 ReadPCMFramesAndMixToOutput(float *dst, ma_uint64 count) {
    if (!m_playing ||
        !m_ready) { // return early, if paused (or not initialized right)
      return 0;
    }

    ma_result result; // init vars
    ma_uint32 tempCapInFrames = TEMP_BUFFER_SIZE / GetChannelCount();
    ma_uint32 totalFramesRead = 0;

    while (totalFramesRead <
           count) { // read in the same temp buffer until requested frames read

      ma_uint64 iSample; // the current input sample id
      ma_uint64 framesReadThisIteration;
      ma_uint32 totalFramesRemaining = count - totalFramesRead;
      ma_uint32 framesToReadThisIteration = tempCapInFrames;

      if (framesToReadThisIteration >
          totalFramesRemaining) { // cap to be read frame amount at remaining
                                  // frame amount
        framesToReadThisIteration = totalFramesRemaining;
      }

      result = ma_decoder_read_pcm_frames(
          &m_decoder, m_tempBuffer, framesToReadThisIteration,
          &framesReadThisIteration); // read the frames from internal decoder
      if (result != MA_SUCCESS ||
          framesReadThisIteration == 0) { // error occurred
        break;
      }

      ma_uint32 channelID = 0; // id of the current input channel
      ma_uint32 frameID = 0;   // id of the current FrameID
      for (iSample = 0; iSample < framesReadThisIteration * GetChannelCount();
           ++iSample) { // loop over all samples from the read frames

        for (int d = 0; d < m_routingMatrix[channelID].size();
             d++) { // loop over the routing matrix
          dst[(totalFramesRead * GetChannelCount()) +
              (frameID * GetChannelCount()) + m_routingMatrix[channelID][d]] +=
              m_tempBuffer[iSample] * m_volume; // add current sample to mix
        }

        channelID++; // increment channel
        if (channelID >=
            GetChannelCount()) { // if all channels of the Frame are processed
                                 // advance ptrs to next
          channelID = 0;
          frameID++;
        }
      }

      totalFramesRead +=
          (ma_uint32)framesReadThisIteration; // increase the read frame count

      if (framesReadThisIteration <
          (ma_uint32)framesToReadThisIteration) { // auto pause if requesting
                                                  // over range
        m_playing = false;
      }
    }

    return totalFramesRead; // return total frames read
  }
};