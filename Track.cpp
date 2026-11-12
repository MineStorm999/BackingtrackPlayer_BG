#include "Track.h"

#include "defines.h"

Track::Track(nlohmann::json &data) {
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
  ma_uint32 channelCount = m_decoder.outputChannels;
  if (routing.size() < channelCount) {
    printf("ERROR routing matrix is missing channels\n");
    // std::cout << routing.dump(4);
    ma_decoder_uninit(&m_decoder);
    return;
  }
  printf("Routed Channels: %u\n", (ma_uint32)routing.size());

  m_routingMatrix.resize(channelCount);

  for (int i = 0; i < channelCount; i++) {
    nlohmann::json &jDstChannels =
        routing[i]; // retrieving per channel route dsts from
                    // json and verifying it
    if (jDstChannels.is_null()) {
      printf("ERROR routing matrix is missing (channel %i)\n", i);
      // std::cout << jDstChannels.dump(4);
      ma_decoder_uninit(&m_decoder);
      return;
    }

    m_routingMatrix[i].resize(jDstChannels.size());
    for (int j = 0; j < m_routingMatrix[i].size(); j++) {
      m_routingMatrix[i][j] = jDstChannels[j];

      if (jDstChannels[j] >= OUT_CHANNEL_COUNT) { // dst channel id out of range
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
  m_ready = true;
  m_playing = true;
}
Track::~Track() { _Uninit(); }
void Track::_Uninit() {
  printf("Deleting\n");
  if (m_ready) {
    ma_decoder_uninit(&m_decoder);
  }
  m_ready = false;
  m_routingMatrix.clear();
}
bool Track::Ready() { return m_ready; }
ma_uint32 Track::GetChannelCount() {
  if (!m_ready) {
    return 0;
  }
  return m_decoder.outputChannels;
}
void Track::Seek(ma_uint64 pos) {
  if (!m_ready) {
    return;
  }
  ma_decoder_seek_to_pcm_frame(&m_decoder, pos);
}
ma_uint64 Track::GetPos() {
  if (!m_ready) {
    return 0;
  }
  ma_uint64 ret = 0;
  ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &ret);
  return ret;
}
bool Track::Playing() { return m_playing; }
bool Track::NeedsProcessing() { return m_playing && m_ready; }
void Track::Pause() { m_playing = false; }
void Track::Play(bool fromBeginning) {
  if (fromBeginning) {
    Seek(0);
  }
  m_playing = true;
}
ma_uint64
Track::ReadPCMFramesAndMixToOutput(Eigen::Map<Eigen::MatrixXf> &outMat,
                                   ma_uint64 count) {
  if (!m_playing ||
      !m_ready) { // return early, if paused (or not initialized right)
    return 0;
  }

  m_tempBuffer.resize(
      std::max((ma_uint64)m_tempBuffer.size(),
               count * GetChannelCount())); // resize read buffer to frame count

  ma_result result; // init vars

  // auto &t = dstMat;
  ma_uint64 read = 0;
  result =
      ma_decoder_read_pcm_frames(&m_decoder, m_tempBuffer.data(), count, &read);

  Eigen::Map<Eigen::MatrixXf> inMat(m_tempBuffer.data(), GetChannelCount(),
                                    read); // create matrix

  inMat *= m_volume;
  for (int in = 0; in < m_routingMatrix.size(); in++) {
    for (ma_uint32 out : m_routingMatrix[in]) {
      outMat.row(out) += inMat.row(in);
    }
  }

  if (read < count) { // auto pause if requesting
                      // over range
    m_playing = false;
  }
  return read; // return total frames read
};
