#include "Song.h"
#include <algorithm>
#include <cstdio>
#include <fstream>

Song::Song(const std::string &dir, const std::string &path) {
  m_dir = dir;
  std::string npath = m_dir + path;
  printf("Path %s\n", npath.c_str());
  std::ifstream f(npath); // validate file
  if (!f.is_open()) {
    return;
  }

  nlohmann::json config = nlohmann::json::parse(f); // parse and validate json
  if (config.is_null()) {
    return;
  }

  if (!_FromJSON(config)) { // load from json
    return;
  }

  m_ready = true;
  m_playing = true;
}
bool Song::_FromJSON(nlohmann::json &config) {
  if (config.is_null()) { // config missing
    printf("ERROR config missing\n");
    return false;
  }

  nlohmann::json &tracksJS = config["tracks"];
  if (!tracksJS.is_array()) { // files missing
    printf("ERROR files missing\n");
    return false;
  }

  tracks.clear();
  tracks.reserve(tracksJS.size()); // Testing
  // int i = 0;
  for (nlohmann::json &f : tracksJS) {
    for (int i = 0; i < 1; i++) {
      tracks.push_back(std::make_shared<Track>(m_dir, f));
      if (!tracks.back()->Valid()) {
        printf("Warning file setup failed/not routed, removing\n");
        tracks.pop_back();
      }
    }
  }
  tracks.shrink_to_fit();
  if (tracks.empty()) {
    printf("Song hasn't got any tracks\n");
    return false;
  }
  return true;
}

ma_uint64 Song::GetProgressInPCMFrames() { return m_sampleProgress; }

void Song::Play() {
  m_playing = true;
  StartFade(1.0, 4800, false, false);
  printf("Play\n");
}
void Song::Pause(bool stop) { StartFade(0.0, 4800, true, stop); }

void Song::StartFade(float target, ma_uint64 fadeSamples, bool shouldStop,
                     bool shouldReverse) {
  m_fadeAmount = (float)(target - m_volume) / (float)fadeSamples;
  m_shouldFade = true;
  m_fadeFramesRemaining = fadeSamples;
  m_shouldStop = shouldStop;
  m_shouldReverse = shouldReverse;
  printf("Stop\n");
}
float Song::GetVolumeForNextSample() {
  if (!m_shouldFade) {
    return m_volume;
  }
  if (!m_playing) {
    return m_volume;
  }
  if (!m_shouldFade) {
    return m_volume;
  }
  m_fadeFramesRemaining--;
  if (m_fadeFramesRemaining < 0) { // fade done

    m_shouldFade = false;
    if (m_shouldStop) {
      m_playing = false;
    }
    if (m_shouldReverse) {
      m_mustReverse = true;
    }

    float vol = m_volume;
    printf("Volume: %f\n", vol);
    return m_volume;
  }
  m_volume = std::clamp(m_volume + m_fadeAmount, 0.0f, 1.0f);
  float vol = m_volume;
  // printf("Volume: %f\n", vol);
  return m_volume;
}
bool Song::Playing() { return m_playing; }

bool Song::MustReverse() {
  if (m_mustReverse) {
    m_mustReverse = false;
    return true;
  }
  return false;
}
void Song::ReverseTracks() {

  for (std::shared_ptr<Track> track : tracks) {
    if (!track) {
      continue;
    }
    track->Seek(0);
    if (Playing()) {
      track->Play();
    }
  }
  printf("Reverse");
}

bool Song::Valid() { return m_ready; }
void Song::_Invalidate() { m_ready = false; };

void Song::MixTracksToOut(Eigen::Map<Eigen::MatrixXf> &outMat,
                          ma_uint64 count) {
  if (!Valid()) {
    return;
  }

  if (!Playing()) {
    return;
  }

  if (MustReverse()) {
    ReverseTracks();
  }
  bool tracksValid = false;  // remains false, if all tracks are invalid
  bool tracksStopped = true; // remains true when all tracks are stopped

  for (std::shared_ptr<Track> t : tracks) {
    if (!t->Valid()) { // track not ready
      continue;
    }
    tracksValid = true;

    if (!t->Playing()) { // track not playing
      continue;
    }

    tracksStopped = false;
    t->ReadPCMFramesAndMixToOutput(outMat, count); // mix track to out
  }
  if (!tracksValid) {
    _Invalidate();
  }
  if (tracksStopped) {
    m_playing = false;
  }

  if (!m_shouldFade) {
    return;
  }
  for (int i = 0; i < count; i++) {
    // volumeVec[i] = song->GetVolumeForNextSample();
    outMat.col(i) *= std::pow(GetVolumeForNextSample(), 2);
  }
}
