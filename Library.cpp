#include "Library.h"
#include <memory>

Library::Library(const std::string &path) {}
std::shared_ptr<Show> Library::GetCurrentShow() {
  if (!m_currentShowNode) {
    return nullptr;
  };
  return m_currentShowNode->own;
}
std::shared_ptr<Show> Library::GetShow(ma_uint32 id) {
  int i = m_currentShowID;
  std::shared_ptr<Node> cur;
  while (i >= 0 && i < m_nodeCount) {

    if (id > i) {

      if (!m_currentShowNode->prev)
        i--;

    } else {
      i++;
    }
  }
}