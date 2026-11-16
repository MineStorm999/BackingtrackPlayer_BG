#include "Show.h"
#include <atomic>
#include <memory>
#include <string>

struct Node {
  std::atomic<std::shared_ptr<Node>> next;
  std::atomic<std::shared_ptr<Show>> prev;
  std::atomic<std::shared_ptr<Node>> own;
};
/**
 * @brief this class holds all shows in its root dir
 *
 */
class Library {
public:
  Library(const std::string &path);

private:
  std::string m_path;
};