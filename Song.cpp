#include "Song.h"
#include <cstdio>

Song::Song(nlohmann::json &config) {
  FromFile(config);
  m_playing = true;
}
void Song::FromFile(nlohmann::json &config) {
  if (config.is_null()) { // config missing
    printf("ERROR config missing\n");
    return;
  }

  nlohmann::json &tracksJS = config["tracks"];
  if (!tracksJS.is_array()) { // files missing
    printf("ERROR files missing\n");
    return;
  }

  tracks.clear();
  tracks.reserve(tracksJS.size()); // Testing
  // int i = 0;
  for (nlohmann::json &f : tracksJS) {
    for (int i = 0; i < 1; i++) {
      tracks.push_back(std::make_shared<Track>(f));
      if (!tracks.back()->Ready()) {
        printf("Warning file setup failed/not routed, removing\n");
        tracks.pop_back();
      }
    }
  }
  tracks.shrink_to_fit();
}
ma_uint64 Song::GetProgressInPCMFrames() { return m_sampleProgress; }
void Song::Play() {
  m_playing = true;
  StartFade(1.0, 48000, false);
}
void Song::Pause() { StartFade(0.0, 48000, true); }
void Song::StartFade(float target, ma_uint64 fadeSamples, bool shouldStop) {
  m_fadeAmount = (float)(target - m_volume) / (float)fadeSamples;
  m_shouldFade = true;
  m_fadeFramesRemaining = fadeSamples;
  m_shouldStop = shouldStop;
}
float Song::GetVolumeForNextSample() {
  if (!m_shouldFade) {
    return m_volume;
  }
  if (!m_playing) {
    return m_volume;
  }

  m_fadeFramesRemaining--;
  if (m_fadeFramesRemaining < 0) { // fade done
    m_shouldFade = false;
    if (m_shouldStop) {
      m_playing = false;
    }
    return m_volume;
  }
  m_volume = m_volume + m_fadeAmount;
  float vol = m_volume;
  // printf("Volume: %f\n", vol);
  return m_volume;
}
bool Song::Playing() { return m_playing; }
