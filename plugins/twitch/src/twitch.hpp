#pragma once
#include "shared/resource_util.hpp"
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <future>
#include <string>
#include <filesystem>
#include <tuple>
#include <mutex>

namespace network {
using refresh_function_t = std::function<
    std::tuple<std::string, std::string, int>(std::string)>;

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
  nlohmann::json GetGameInfo(std::string const& game_name,
      std::string const& client_id,
      std::string const& jwt);

private:
  std::unordered_map<std::string, nlohmann::json> games_;
};

class TwitchToken {
public:
  TwitchToken() = delete;
  TwitchToken(std::string access_token,
      std::string refresh_token,
      int expires_in,
      refresh_function_t refresh_fun);
  ~TwitchToken();

  std::string const& GetAccessToken() {
    std::unique_lock lock(mutex_);
    return access_token_;
  }

private:
  std::string access_token_;
  std::string refresh_token_;
  bool quit_{ false };
  std::mutex mutex_;
  std::condition_variable cv_;
  std::future<void> refresher_;
};

class TwitchClient {
public:
  TwitchClient(std::filesystem::path data_dir);
  ~TwitchClient();

  void StartListen(std::string client_id,
      std::string secret,
      std::string ip,
      int port);
  void Stop();

  bool Request(std::string const& cmd,
      nlohmann::json const& params = nlohmann::json::object());

  [[nodiscard]] bool IsAuthenticated() const noexcept {
    return token_ && !token_->GetAccessToken().empty();
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
      const std::string& title = {}) const;

  [[nodiscard]] std::string const& GetAccessToken() const {
    return token_ ? token_->GetAccessToken() : "";
  }

private:
  void RegisterEndpoints();

  void Authorize(const httplib::Request& req, httplib::Response& res);
  void Console(const httplib::Request& req, httplib::Response& res);

  void ExchangeCodeWithToken(std::string const& code, httplib::Response& res);
  std::tuple<std::string, std::string, int> RefreshToken(
      std::string const& refresh_token);

  std::string GetContent(int resource_id, const nlohmann::json& vars) const;

  std::filesystem::path data_dir_;
  std::thread runner_;
  std::string client_id_;
  std::string secret_;
  std::string ip_;
  int port_{};
  bool running_{};
  std::string state_;
  std::string redir_url_;

  httplib::Server server_;
  std::unique_ptr<TwitchUser> user_;
  std::unique_ptr<TwitchToken> token_;
  TwitchGame game_;
  windows::EmbeddedResource resource_;

  std::unordered_map<std::string, std::function<void(nlohmann::json const&)>>
      commands_;
  void OpenConsole(nlohmann::json const& params);
  void OpenFile(nlohmann::json const& params);
};
}  // namespace network
