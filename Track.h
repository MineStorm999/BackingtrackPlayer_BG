#pragma once
#include "Eigen/Core"
#include "json.hpp"
#include "miniaudio.h"
#include <vector>

#define TEMP_BUFFER_SIZE 4096

/**
 * @brief This class is a wrapper around ma_decoder for safer use
 *
 */
class Track {
public:
  Track(const std::string &dir, nlohmann::json &data);

  ~Track();

private:
  ma_decoder m_decoder;
  ma_bool32 m_playing{false};
  ma_bool32 m_ready{false};
  ma_uint64 m_pos{0}; // in samples

  float m_volume{1.0f};
  std::vector<std::vector<ma_uint32>>
      m_routingMatrix; // in channel / outchannel [in][out]

  std::vector<float> m_tempBuffer; // to store read data

  void _Uninit();

public:
  bool Valid();

  ma_uint32 GetChannelCount();

  // seeking
  void Seek(ma_uint64 pos);

  // get progress in pcm frames
  ma_uint64 GetPos();

  // is playing?
  bool Playing();

  bool NeedsProcessing();

  // pause playback
  void Pause();
  // start playback
  void Play(bool fromBeginning = false);

  /**
   * @brief Read the requested pcm frames and mix them to the output buffer
   * @in: dst = outputbuffer, count output count in frames
   */
  ma_uint64 ReadPCMFramesAndMixToOutput(Eigen::Map<Eigen::MatrixXf> &outMat,
                                        ma_uint64 count);
};