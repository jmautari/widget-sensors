#pragma once

#include <chrono>
#include <unordered_map>
#include <string>

namespace core {
class ProcessTracker {
public:
  void Add(int pid, std::string process_name);

  bool Delete(int pid);

  [[nodiscard]] int GetPidByProcessName(std::string const& process_name) const;

  [[nodiscard]] auto GetSize() const noexcept {
    return list_.size();
  }

  template<typename T>
  T GetElapsedTime(int pid) {
    auto&& it = list_.find(pid);
    if (it == list_.end())
      return {};

    auto start_time = it->second.second;
    return std::chrono::duration_cast<T>(
        std::chrono::system_clock::now() - start_time);
  }

private:
  std::unordered_map<int,
                    std::pair<std::string,
                        std::chrono::system_clock::time_point>> list_;
};
}  // namespace core