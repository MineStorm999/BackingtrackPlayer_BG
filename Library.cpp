#include "Library.h"
#include <memory>

Library::Library(const std::string &path) {
  // todo scan dir for shows
  m_valid = true;
}

std::shared_ptr<Show> Library::GetShow(ma_uint32 id) {
  if (id > m_shows.size()) {
    return nullptr;
  }
  if (!Valid()) {
    return nullptr;
  }
  return m_shows[id];
}
bool Library::Valid() { return m_valid; }
void Library::_Invalidate() { m_valid = false; };
