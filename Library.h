#include "Show.h"
#include <atomic>
#include <memory>
#include <string>

struct Node {
  std::atomic<std::shared_ptr<Node>> next;
  std::atomic<std::shared_ptr<Node>> prev;
  std::shared_ptr<Show> own;
};
/**
 * @brief this class holds all shows in its root dir
 *
 */
class Library {
public:
  Library(const std::string &path);

  std::shared_ptr<Show> GetCurrentShow();
  std::shared_ptr<Show> GetShow(ma_uint32 id);

private:
  std::string m_path;

  std::shared_ptr<Node> m_firstShowNode{nullptr};
  ma_uint32 m_nodeCount{0};

  // thread UNSAFE!!!! must only be used by audio thread
  ma_uint32 m_currentShowID;
  // thread UNSAFE!!!! must only be used by audio thread
  std::shared_ptr<Node> m_currentShowNode;
};