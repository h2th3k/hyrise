#include "log_manager.hpp"

namespace opossum {

void LogManager::add_message(const std::string& sender, const std::string& message) {
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
  const LogEntry log_entry {now, sender, message};
  _log_entries.emplace_back(log_entry);
}

const tbb::concurrent_vector<LogEntry>& LogManager::log_entries() const {
  return _log_entries;
}

}  // namespace opossum