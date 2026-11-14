#include "Show.h"
#include "json.hpp"
#include <cstdio>
#include <fstream>
#include <memory>

Show::Show(const std::string &path) {
  std::ifstream f(path); // validate file
  if (!f.is_open()) {
    return;
  }

  nlohmann::json config = nlohmann::json::parse(f); // parse and validate json
  if (config.is_null()) {
    return;
  }

  nlohmann::json &jName = config["name"]; // validate name (and setting it)
  if (!jName.is_string()) {
    return;
  }
  m_name = jName;

  nlohmann::json &jSongs = config["songs"];
  if (!jSongs.is_array()) {
    return;
  }
  m_songs.reserve(jSongs.size());
  for (nlohmann::json &jSong : jSongs) {
    m_songs.push_back(std::make_shared<Song>(jSong));
    if (!m_songs.back()) {
      printf("Song is invalid, disposing...");
      m_songs.pop_back();
      continue;
    }
    if (m_songs.back()->tracks.empty()) {
      printf("Song has no tracks, disposing...");
      m_songs.pop_back();
      continue;
    }
  }
  m_songs.shrink_to_fit();
  if (m_songs.empty()) {
    return;
  }
}

// get current song
const std::shared_ptr<Song> Show::GetSong() { return m_songs[m_songID]; }

// move to next song
void Show::Next() {
  m_songID++;
  if (m_songID >= m_songs.size()) {
    m_songID = 0;
  }
}

// move to previous song
void Show::Prev() {
  m_songID--;
  if (m_songID < 0) {
    m_songID = m_songs.size() - 1;
  }
}

// check if show is valid
bool Show::Valid() { return m_ready; }

// get the name
const std::string &Show::GetName() { return m_name; };
