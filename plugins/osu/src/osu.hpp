/**
 * Widget Sensors
 * Osu! plug-in
 * Copyright (C) 2021-2023 John Mautari - All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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

class OsuUser {
public:
  OsuUser(std::string const& client_id, std::string const& jwt) {
    GetOsuUserInfo(client_id, jwt);
  }

  [[nodiscard]] const nlohmann::json& GetUserInfo() const {
    return user_info_;
  }

private:
  nlohmann::json user_info_;

  void GetOsuUserInfo(std::string const& client_id, std::string const& jwt);
};

class OsuToken {
public:
  OsuToken() = delete;
  OsuToken(std::string access_token,
      std::string refresh_token,
      int expires_in,
      refresh_function_t refresh_fun);
  ~OsuToken();

  [[nodiscard]] std::string const& GetAccessToken() {
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

class OsuClient {
public:
  OsuClient(std::filesystem::path data_dir);
  ~OsuClient();

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

  [[nodiscard]] std::string const& GetAccessToken() const {
    return token_ ? token_->GetAccessToken() : "";
  }

private:
  void RegisterEndpoints();

  void Authorize(const httplib::Request& req, httplib::Response& res);
  void Console(const httplib::Request& req, httplib::Response& res);

  void ExchangeCodeWithToken(std::string const& code, httplib::Response& res);
  [[nodiscard]] std::tuple<std::string, std::string, int> RefreshToken(
      std::string const& refresh_token);

  [[nodiscard]] std::string GetContent(int resource_id,
      const nlohmann::json& vars) const;

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
  std::unique_ptr<OsuUser> user_;
  std::unique_ptr<OsuToken> token_;
  windows::EmbeddedResource resource_;

  std::unordered_map<std::string, std::function<void(nlohmann::json const&)>>
      commands_;
  void OpenConsole(nlohmann::json const& params);
  void OpenFile(nlohmann::json const& params);
};
}  // namespace network
