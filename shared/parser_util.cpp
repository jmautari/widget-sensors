#include "parser_util.hpp"

namespace util {
Parser::Parser(std::string const& var_pattern) : regex_{ var_pattern } {
}

void Parser::Replace(std::string& text, nlohmann::json const& vars) const {
  std::smatch m;
  while (std::regex_search(text, m, regex_) && m.size() == 2) {
    auto var = m[1].str();
    text.replace(m.position(), m.length(), GetVar(vars, var));
  }
}

std::string Parser::GetVar(nlohmann::json const& vars,
    std::string const& var) const {
  if (vars.contains(var)) {
    if (vars[var].is_string())
      return vars[var].get<std::string>();
    else if (vars[var].is_number_integer())
      return std::to_string(vars[var].get<int>());
    else if (vars[var].is_number_float())
      return std::to_string(vars[var].get<float>());
    else if (vars[var].is_boolean())
      return vars[var].get<bool>() ? "true" : "false";
  }

  return "";
}
}  // namespace util
