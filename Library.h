#include "Show.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief this class holds all shows in its root dir
 *
 */
class Library {
public:
  Library(const std::string &path);

  std::shared_ptr<Show> GetShow(ma_uint32 id);

  bool Valid();
  void _Invalidate();

private:
  std::atomic<bool> m_valid;

  std::string m_path;

  // must not be changed at runtime
  std::vector<std::shared_ptr<Show>> m_shows;
};