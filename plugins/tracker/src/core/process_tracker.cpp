#include "process_tracker.hpp"

namespace core {
void ProcessTracker::Add(int pid, std::string process_name) {
  list_[pid] = std::make_pair(
      std::move(process_name), std::chrono::system_clock::now());
}

bool ProcessTracker::Delete(int pid) {
  if (auto&& it = list_.find(pid); it != list_.end()) {
    list_.erase(it);
    return true;
  }

  return false;
}

int ProcessTracker::GetPidByProcessName(
    std::string const& process_name) const {
  int pid{};
  auto it = std::find_if(list_.begin(), list_.end(), [&](auto&& i) {
    if (i.second.first != process_name)
      return false;

    pid = i.first;
    return true;
  });
  if (it == list_.end())
    return {};

  return pid;
}
}  // namespace core