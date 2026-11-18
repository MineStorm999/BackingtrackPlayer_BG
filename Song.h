#include "Track.h"
#include "miniaudio.h"
#include <atomic>

class Song {
public:
  Song(const std::string &dir, const std::string &path);

  bool _FromJSON(nlohmann::json &config);

  ma_uint64 GetProgressInPCMFrames();

  void Play();
  void Pause(bool stop = false);

  std::vector<std::shared_ptr<Track>> tracks;

  void StartFade(float target, ma_uint64 fadeSamples, bool shouldStop,
                 bool shouldReverse);

  // TODO mabybe optimize
  float GetVolumeForNextSample();

  bool Playing();
  bool MustReverse();

  void ReverseTracks();

  bool Valid();
  void _Invalidate();

  void MixTracksToOut(Eigen::Map<Eigen::MatrixXf> &outMat, ma_uint64 count);

  bool Finished();

private:
  std::atomic<bool> m_ready{false};
  std::string m_dir{};
  std::atomic<ma_uint64> m_sampleProgress; // in pcm frames
  std::atomic<bool> m_playing;
  std::atomic<int> m_fadeFramesRemaining;
  std::atomic<float> m_fadeAmount;
  std::atomic<bool> m_shouldFade;

  std::atomic<bool> m_finished{false};

  std::atomic<bool> m_shouldStop;
  std::atomic<bool> m_shouldReverse;
  std::atomic<bool>
      m_mustReverse; // used by audio thread, to know if to reverse

  std::atomic<float> m_volume;
};