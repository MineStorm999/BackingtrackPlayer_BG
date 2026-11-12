#include "Track.h"
#include "miniaudio.h"
#include <atomic>

class Song {
public:
  Song(nlohmann::json &config);

  void FromFile(nlohmann::json &config);

  ma_uint64 GetProgressInPCMFrames();

  void Play();

  void Pause();

  std::vector<std::shared_ptr<Track>> tracks;

  void StartFade(float target, ma_uint64 fadeSamples, bool shouldStop);

  // TODO mabybe optimize
  float GetVolumeForNextSample();

  bool Playing();

private:
  std::atomic<ma_uint64> m_sampleProgress; // in pcm frames
  std::atomic<bool> m_playing;
  std::atomic<int> m_fadeFramesRemaining;
  std::atomic<float> m_fadeAmount;
  std::atomic<bool> m_shouldFade;
  std::atomic<bool> m_shouldStop;

  /*std::atomic<*/ float /*>*/ m_volume;
};