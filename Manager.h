#pragma once
#include "Library.h"
#include <atomic>
#include <mutex>
#include <vector>

class Manager {
public:
  void Start() { playReq = true; }

  void Stop() { stopReq = true; }
  void OnLibRemoved() { m_canStart = false; }

  void DisposeInvalidShows() {
    std::lock_guard<std::mutex> lock(m_showMtx);
    // TODO lock show vector
    /*auto ret = std::remove_if(
        m_shows.begin(), m_shows.begin(),
        [](std::shared_ptr<Show> s) { return s->Valid() && s->used; });*/
    for (int i = 0; i < m_shows.size(); i++) {
      if (m_shows[i]) {
        if (m_shows[i]->Valid() || m_shows[i]->used > 0) {
          continue;
        }
      }
      m_cur.compare_exchange_strong(m_shows[i], nullptr);
      delete m_shows[i];
      m_shows.erase(m_shows.begin() + i);
      i--;
    }
  };

  // must only be called by audio thread
  void UseNextShow() {
    Show *next = m_next.load();
    if (!next) { // next song not selected/present
      return;
    }

    if (!next->Valid()) { // next song is invalid
      next->used--;       // not used
      return;
    }
    Show *cur = m_cur.load();
    if (!cur) {
      return;
    }

    cur->used--; // not used by audio thread
    m_cur = next;
    m_next = nullptr;
  };

  Show *GetCurrentShow() {
    Show *cur = m_cur.load();
    if (!cur || cur->Valid()) {
      UseNextShow();
    }
    return m_cur;
  };

  std::atomic<bool> playReq;
  std::atomic<bool> stopReq;

  std::atomic<bool> disposeReq;

private:
  // m_cur must have "used" set before using (when m_shows is locked so it
  // can't get deleted)
  std::atomic<Show *> m_cur;
  // next meaning the next selected show
  // m_next must have "used" set before using (when m_shows is locked so it
  // can't get deleted)
  std::atomic<Show *> m_next;

  // needs to bee locked before modify!!!
  std::vector<Show *> m_shows;
  std::mutex m_showMtx;
  std::atomic<bool> m_stopped;

  std::atomic<bool> m_canStart;
};