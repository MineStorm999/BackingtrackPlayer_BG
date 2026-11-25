#include "Song.h"
#include <atomic>
#include <memory>
#include <vector>

class Show {
public:
  Show(const std::string &dir, const std::string &path);

  const std::shared_ptr<Song> GetSong();
  void Next();
  void Prev();

  // should be called before accessing
  bool Valid();

  const std::string &GetName();

  void _MakeInvalid() { m_ready = false; };

  std::atomic<int> used{0}; // prevent deletetion when used

private:
  std::string m_dir{};
  std::shared_ptr<Song> m_currentSong;
  std::atomic<bool> m_ready{false};
  std::string m_name{""};
  std::vector<std::string> m_songs;
  std::atomic<int> m_songID{0};
  std::atomic<int> m_currentSongID{0};
};