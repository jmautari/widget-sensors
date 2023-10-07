#pragma once
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <string>

namespace network {
class TwitchUser {
public:
  TwitchUser(std::string const& client_id, std::string const& jwt) {
    GetTwitchUserInfo(client_id, jwt);
  }

  const nlohmann::json& GetUserInfo() const {
    return user_info_;
  }

private:
  nlohmann::json user_info_;

  void GetTwitchUserInfo(std::string const& client_id, std::string const& jwt);
};

class TwitchGame {
public:
  TwitchGame() = default;

  nlohmann::json GetGameInfo(std::string const& game_name,
      std::string const& client_id,
      std::string const& jwt);

private:
  std::unordered_map<std::string, nlohmann::json> games_;
};

class TwitchClient {
public:
  TwitchClient() = default;
  ~TwitchClient();

  void StartListen(std::string client_id,
      std::string secret,
      std::string ip,
      int port);
  void Stop();

  bool Request(std::string const& cmd,
      nlohmann::json const& params = nlohmann::json::object());

  [[nodiscard]] bool IsAuthenticated() const noexcept {
    return !jwt_.empty();
  }

  [[nodiscard]] std::string GetAuthenticationUrl();

  [[nodiscard]] std::string const& GetRedirUrl() const {
    return redir_url_;
  }

  [[nodiscard]] nlohmann::json const& GetUserInfo() const {
    return user_->GetUserInfo();
  }

  [[nodiscard]] nlohmann::json GetGameInfo(const std::string& game_name);

  void SetBroadcastInfo(const std::string& game_id,
      const std::string& title = {});

private:
  void RegisterEndpoints();

  void Authorize(const httplib::Request& req, httplib::Response& res);
  void ExchangeCodeWithToken(std::string const& code, httplib::Response& res);

  std::thread runner_;
  std::string client_id_;
  std::string secret_;
  std::string ip_;
  int port_{};
  bool running_{};
  std::string jwt_;
  std::string state_;
  std::string redir_url_;

  httplib::Server server_;
  std::unique_ptr<TwitchUser> user_;
  TwitchGame game_;
};
}  // namespace network
