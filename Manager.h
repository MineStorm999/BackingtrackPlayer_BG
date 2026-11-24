#include "Library.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

class Manager {
  void Start() { playReq = true; }

  void Stop() { stoppReq = true; }
  void OnLibRemoved() { m_canStart = false; }

  void DisposeInvalidShows() {
    // TODO lock show vector
    auto ret = std::remove_if(
        m_shows.begin(), m_shows.begin(),
        [](std::shared_ptr<Show> s) { return s->Valid() && s->used; });
  };

  std::atomic<bool> playReq;
  std::atomic<bool> stoppReq;

  std::atomic<bool> disposeReq;

private:
  std::shared_ptr<Show> m_cur;
  std::shared_ptr<Show> m_next;
  // needs to bee locked before modify!!!
  std::vector<std::shared_ptr<Show>> m_shows;
  std::atomic<bool> m_stopped;

  std::atomic<bool> m_canStart;
};