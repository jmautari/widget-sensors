#pragma once
#include "nlohmann/json.hpp"
#include <atomic>
#include <filesystem>
#include <optional>
#include <shared_mutex>

namespace core {
class SimpleDb {
public:
  [[nodiscard]] virtual bool Load(std::filesystem::path path,
      bool create_always = false);

  [[nodiscard]] virtual bool Save(bool pretty_format = true) {
    return SaveAs(path_, pretty_format);
  }

  [[nodiscard]] virtual bool SaveAs(std::filesystem::path const& path,
      bool pretty_format = true);

  virtual void Clear() {
    data_.clear();
  }

  [[nodiscard]] virtual nlohmann::json& Find(
      std::function<bool(nlohmann::json const&)> const& predicate);

  [[nodiscard]] nlohmann::json& GetData() {
    if (!locked_)
      throw new std::runtime_error("Data must be locked");

    return data_;
  }

  [[nodiscard]] bool Add(nlohmann::json obj);

  [[nodiscard]] bool TryLock() {
    if (locked_)
      return false;

    if (mutex_.try_lock()) {
      locked_ = true;
      return true;
    }

    return false;
  }

  void Unlock() {
    if (!locked_)
      return;

    locked_ = false;
    mutex_.unlock();
  }

protected:
  std::filesystem::path path_;
  nlohmann::json data_;
  nlohmann::json empty_;

private:
  std::shared_mutex mutex_;
  std::atomic<bool> locked_;
};
}  // namespace core
