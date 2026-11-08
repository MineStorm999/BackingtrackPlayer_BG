#pragma once
#include "defines.h"
#include "miniaudio.h"
#include "thirdparty/json.hpp"
#include <cstdlib>
#include <nlohmann/json_fwd.hpp>
#include <string>

#define TEMP_BUFFER_SIZE 4096

struct ChannelRoute {
  ma_uint32 routeCount;
  ma_uint32 *dstChannels;
};

/**
 * @brief This class is a wrapper around ma_decoder for safer use
 *
 */
class Decoder {
public:
  Decoder(nlohmann::json &data) {

    std::string path = data["path"]; // retrieve json data

    auto conf = ma_decoder_config_init(ma_format_f32, 0,
                                       SAMPLE_RATE); // init internal decoder
    ma_result r = ma_decoder_init_file(path.c_str(), &conf, &m_decoder);
    if (r != MA_SUCCESS) {
      printf("ERROR internal decoder init failed");
      return;
    }

    nlohmann::json routing =
        (data["routing"]); // reading and verifying routing matrix from json
    if (routing.is_array()) {
      printf("ERROR routing matrix is in wrong syntax/missing");
      return;
    }
    if (routing.size() < GetChannelCount()) {
      printf("ERROR routing matrix is missing channels");
      return;
    }

    m_routingMatrix = (ChannelRoute *)malloc(
        sizeof(ChannelRoute) *
        GetChannelCount()); // creating routing matrix in memory

    for (int i = 0; i < GetChannelCount(); i++) {
      nlohmann::json &jDstChannels =
          routing[i]["dstChannels"]; // retrieving per channel rout dsts from
                                     // json and verifying it
      if (!jDstChannels.is_array()) {
        printf("ERROR routing matrix is in wrong syntax/missing (channel %i)",
               i);
        return;
      }

      m_routingMatrix[i].routeCount = jDstChannels.size();
      m_routingMatrix[i].dstChannels = (ma_uint32 *)malloc(
          sizeof(ma_uint32) * m_routingMatrix[i].routeCount);
      for (int j = 0; j < m_routingMatrix[i].routeCount; j++) {
        m_routingMatrix[i].dstChannels[j] = jDstChannels[i];
        if (jDstChannels[j] >= OUT_CHANNEL_COUNT) {
          printf("ERROR routing matrix is in wrong: only 8 outputs max");
          return;
        }
      }
    }
  }
  Decoder() {};

  ~Decoder() { delete m_routingMatrix; };

  // float *outData; // this data is only valid immediately after calling
  //  ReadPCMFrames()

private:
  ma_decoder m_decoder;
  ma_bool32 m_playing{false};
  ma_bool32 m_ready{false};
  ma_uint64 m_pos{0}; // in samples

  ChannelRoute *m_routingMatrix;

  float m_tempBuffer[TEMP_BUFFER_SIZE]; // to store read data

public:
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
  ma_uint64 ReadPCMFrames(void *dst, ma_uint64 count) {
    if (!m_playing ||
        !m_ready) { // return early, if paused (or not initialized right)
      return 0;
    } /*
     if (data_count < count) {
       outData = malloc(m_channelCount *);
     }*/
    ma_uint64 read = 0;

    if (read < count) { // auto pause if requesting over range
      m_playing = false;
    }

    ma_result result;
    ma_uint32 tempCapInFrames = TEMP_BUFFER_SIZE / m_channelCount;
    ma_uint32 totalFramesRead = 0;

    while (totalFramesRead < count) {
      ma_uint64 iSample;
      ma_uint64 framesReadThisIteration;
      ma_uint32 totalFramesRemaining = count - totalFramesRead;
      ma_uint32 framesToReadThisIteration = tempCapInFrames;
      if (framesToReadThisIteration > totalFramesRemaining) {
        framesToReadThisIteration = totalFramesRemaining;
      }

      result = ma_decoder_read_pcm_frames(&m_decoder, m_tempBuffer,
                                          framesToReadThisIteration,
                                          &framesReadThisIteration);
      if (result != MA_SUCCESS || framesReadThisIteration == 0) {
        break;
      }

      ma_uint32 channel = 0;
      for (iSample = 0; iSample < framesReadThisIteration * m_channelCount;
           ++iSample) {

        channel++;
        if (channel >= m_channelCount) {
          channel = 0;
        }
      }

      totalFramesRead += (ma_uint32)framesReadThisIteration;

      if (framesReadThisIteration < (ma_uint32)framesToReadThisIteration) {
        break; /* Reached EOF. */
      }
    }

    return totalFramesRead;
    return read;
  }
};