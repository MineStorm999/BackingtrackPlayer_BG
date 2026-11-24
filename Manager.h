#pragma once
#include "Library.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

class Manager {
public:
  void Start() { playReq = true; }

  void Stop() { stopReq = true; }
  void OnLibRemoved() { m_canStart = false; }

  void DisposeInvalidShows() {
    // TODO lock show vector
    auto ret = std::remove_if(
        m_shows.begin(), m_shows.begin(),
        [](std::shared_ptr<Show> s) { return s->Valid() && s->used; });
  };

  // must only be called by audio thread
  void UseNextShow() {
    Show *next = m_next.load();
    if (!next) {
      return;
    }
    if (!next->Valid()) {
      return;
    }
    Show *cur = m_cur.load();
    if (!cur) {
      return;
    }
    cur->used = false; // safe to delete
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
  std::vector<std::shared_ptr<Show>> m_shows;

  std::atomic<bool> m_stopped;

  std::atomic<bool> m_canStart;
};