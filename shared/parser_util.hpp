#pragma once
#include "nlohmann/json.hpp"
#include <string>
#include <regex>

namespace util {
class Parser {
public:
  Parser(std::string const& var_pattern = R"(\$\{([^}]+)\})");

  void Replace(std::string& text, nlohmann::json const& vars) const;
  std::string GetVar(nlohmann::json const& vars, std::string const& var) const;

private:
  std::regex regex_;
};
}  // namespace util
