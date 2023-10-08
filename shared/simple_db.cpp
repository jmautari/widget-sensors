#include "simple_db.hpp"
#include <execution>
#include <fstream>

namespace core {
bool SimpleDb::Load(std::filesystem::path path, bool create_always) {
  std::unique_lock lock(mutex_);
  path_ = std::move(path);

  if (!std::filesystem::exists(path_)) {
    if (!create_always)
      return false;

    data_ = nlohmann::json::object();
    data_["data"] = nlohmann::json::array();

    lock.unlock();
    return Save();
  }

  std::ifstream file(path_);
  if (!file.is_open())
    return false;

  try {
    data_ = nlohmann::json::parse(file);
    return true;
  } catch (nlohmann::json::parse_error const& e) {
  } catch (...) {
  }
  return false;
}

bool SimpleDb::SaveAs(std::filesystem::path const& path, bool pretty_format) {
  if (locked_)
    return false;

  std::unique_lock lock(mutex_);
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open())
    return false;

  file << data_.dump(pretty_format ? 2 : 0);
  return true;
}

bool SimpleDb::Add(nlohmann::json obj) {
  if (!TryLock())
    return false;

  bool res{};
  try {
    data_["data"].push_back(std::move(obj));
    res = true;
  } catch (...) {
  }
  Unlock();
  return res;
}

nlohmann::json& SimpleDb::Find(
    std::function<bool(nlohmann::json const&)> const& predicate) {
  if (auto it = std::find_if(std::execution::par, data_["data"].begin(),
          data_["data"].end(), [&predicate](auto&& i) { return predicate(i); });
      it != data_["data"].end()) {
    return *it;
  }

  return empty_;
}
}  // namespace core
