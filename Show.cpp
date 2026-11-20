#include "Show.h"
#include "json.hpp"
#include <fstream>
#include <memory>
#include <thread>

Show::Show(const std::string &dir, const std::string &path) {
  m_dir = dir;
  std::ifstream f(m_dir + path); // validate file
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

  nlohmann::json &jSongs = config["songs"]; // get the song list
  if (!jSongs.is_array()) {
    return;
  }

  m_songs.reserve(jSongs.size());

  for (nlohmann::json &jSong : jSongs) { // copy the file paths
    if (!jSong.is_string()) {
      continue;
    }
    m_songs.push_back((jSong));
  }
  m_songs.shrink_to_fit();
  if (m_songs.empty()) {
    return;
  }
  m_ready = true;
  auto s = GetSong();
  if (!s) {
    return;
  }
  s->Play();
}

// get current song returns nullptr if not valid
const std::shared_ptr<Song> Show::GetSong() {
  if (!Valid()) { // invalid
    return nullptr;
  }

  if (m_currentSongID != m_songID) { // song changed
    int songID = m_songID;
    if (songID < m_songs.size() && songID >= 0) { // new song id valid
      m_currentSongID = songID;
      m_currentSong = nullptr;
    } else {
      songID = m_currentSongID;
      m_songID = songID;
    }
  }

  if (!m_currentSong) {
    std::string path =
        (m_dir +
         m_songs[m_currentSongID]); // determine name and dir for the song
    std::string dir = path.substr(0, path.find_last_of("/") + 1);
    std::string name = path.substr(path.find_last_of("/") + 1);
    m_currentSong = std::make_shared<Song>(dir, name);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(1000)); // TODO remove DEBUG
  }
  if (!m_currentSong->Valid()) {
    return nullptr;
  }
  return m_currentSong;
}

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
