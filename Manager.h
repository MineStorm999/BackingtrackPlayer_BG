#include "Library.h"
#include <atomic>

class Manager {
  bool Start() {
    if (!m_canStart) {
      return false;
    }
    return true;
  }

  void Stop() { m_stopped = true; }
  void OnLibRemoved() { m_canStart = false; }

private:
  std::shared_ptr<Show> m_cur;
  std::atomic<bool> m_stopped;
  std::atomic<bool> m_canStart;
};