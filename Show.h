#include "Song.h"
#include <atomic>
#include <memory>
#include <vector>
class Show {
public:
  Show(const std::string &path);

  const std::shared_ptr<Song> GetSong();
  void Next();
  void Prev();

  bool Valid();

  const std::string &GetName();

private:
  bool m_ready{false};
  std::string m_name{""};
  std::vector<std::shared_ptr<Song>> m_songs;
  std::atomic<int> m_songID{0};
};