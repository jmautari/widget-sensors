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
#include "osu.hpp"
#include "shared/logger.hpp"
#include "shared/string_util.h"
#include "shared/parser_util.hpp"
#include "shared/shell_util.hpp"
#include "../resources/win/resource.h"
#include "fmt/format.h"
#include <execution>
#include <random>
#include <regex>
#include <sstream>

#define ADD_COMMAND(cmd) commands_[#cmd] = [this](auto&& json) { cmd(json); }

extern HINSTANCE hInstance;

namespace network {
namespace resource {
constexpr int kConsoleHtml = IDC_CONSOLE_HTML;
}  // namespace resource

struct Osu {
  struct Host {
    static constexpr char kIdHost[] = "https://osu.ppy.sh";
    static constexpr char kApiHost[] = "https://osu.ppy.sh";
  };
  struct Endpoint {
    static constexpr char kAuth[] = "/oauth/authorize";
    static constexpr char kGetToken[] = "/oauth/token";
    static constexpr char kUserMe[] = "/api/v2/me";
  };
  static constexpr char kScope[] = "public+identify";
};

constexpr char kTextPlain[] = "text/plain";
constexpr char kTextHtml[] = "text/html";
constexpr char kApplicationUrlEncode[] = "application/x-www-form-urlencoded";
constexpr char kApplicationJson[] = "application/json";

OsuClient::OsuClient(std::filesystem::path data_dir)
    : data_dir_(data_dir), resource_{ hInstance } {
  LOG(INFO) << __FUNCTION__;
  ADD_COMMAND(OpenConsole);
  ADD_COMMAND(OpenFile);
}

OsuClient::~OsuClient() {
  LOG(INFO) << __FUNCTION__;
  Stop();
}

void OsuClient::Stop() {
  if (server_.is_running())
    server_.stop();

  if (runner_.joinable())
    runner_.join();
}

void OsuClient::StartListen(std::string client_id,
    std::string secret,
    std::string ip,
    int port) {
  client_id_ = std::move(client_id);
  secret_ = std::move(secret);
  ip_ = std::move(ip);
  port_ = port;
  redir_url_ = fmt::format("http://localhost:{}/authorize", port_);

  if (server_.is_running()) {
    Stop();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  } else {
    RegisterEndpoints();
  }

  runner_ = std::thread([this] {
    if (!server_.listen(ip_, port_))
      LOG(ERROR) << "Cannot start listening";
  });
}

std::string OsuClient::GetAuthenticationUrl() {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int> dist(0, 99);
  std::ostringstream s;
  for (int i = 0; i < 16; i++)
    s << dist(mt);

  state_ = fmt::format("{}", std::hash<std::string>{}(s.str()));
  return fmt::format(
      "{}{}?response_type=code&client_id={}&redirect_uri={}&scope={}&state={}",
      Osu::Host::kIdHost, Osu::Endpoint::kAuth, client_id_, GetRedirUrl(),
      Osu::kScope, state_);
}

bool OsuClient::Request(std::string const& cmd,
    nlohmann::json const& params) {
  try {
    if (auto fn = commands_.find(cmd); fn != commands_.end())
      fn->second(params);

    return true;
  } catch (...) {
    LOG(ERROR) << "Error processing request!";
  }
  return false;
}

void OsuClient::OpenConsole(nlohmann::json const& params) {
  static_cast<void>(util::OpenViaShell(
      L"http://localhost:" + std::to_wstring(port_) + L"/console"));
}

void OsuClient::OpenFile(nlohmann::json const& params) {
  std::filesystem::path file;
  try {
    std::error_code ec;
    file = data_dir_ / params["file"];
    if (std::filesystem::exists(file, ec)) {
      static_cast<void>(util::OpenViaShell(L"file:///" + file.wstring()));
    } else {
      LOG(ERROR) << "File " << file.u8string() << " does not exist";
    }
  } catch (...) {
    LOG(ERROR) << "Error opening file " << file.u8string();
  }
}

void OsuClient::Authorize(const httplib::Request& req,
    httplib::Response& res) {
  /*
    code=gulfwdmys5lsm6qyz4xiz9q32l10
    &state=c3ab8aa609ea11e793ae92361f002671
  */
  std::string code = req.get_param_value("code");
  std::string state = req.get_param_value("state");

  if (code.empty() || state != state_) {
    if (state != state_)
      LOG(ERROR) << "Empty or mismatched state value";

    res.set_content("Access denied or invalid response", kTextPlain);
    return;
  }

  ExchangeCodeWithToken(code, res);
}

void OsuClient::ExchangeCodeWithToken(std::string const& code,
    httplib::Response& res) {
  /*
    client_id=hof5gwx0su6owfnys0yan9c87zr6t
    &client_secret=41vpdji4e9gif29md0ouet6fktd2
    &code=gulfwdmys5lsm6qyz4xiz9q32l10
    &grant_type=authorization_code
    &redirect_uri=http://localhost:3000
  */
  httplib::Client cli(Osu::Host::kIdHost);
  std::ostringstream o;
  o << "client_id=" << client_id_ << "&client_secret=" << secret_
    << "&code=" << code << "&grant_type=authorization_code"
    << "&redirect_uri=http://localhost:" << port_ << "/authorize";
  auto response = cli.Post(
      Osu::Endpoint::kGetToken, o.str(), kApplicationUrlEncode);

  if (response && response->status == 200) {
    try {
      auto json = nlohmann::json::parse(response->body);
      if (json.contains("access_token") && json["access_token"].is_string()) {
        token_ = std::make_unique<OsuToken>(json["access_token"],
            json["refresh_token"], json["expires_in"],
            [&](auto refresh_token) { return RefreshToken(refresh_token); });
        user_ = std::make_unique<OsuUser>(
            client_id_, token_->GetAccessToken());

        res.set_content("You can close this tab now", kTextPlain);
        res.status = 200;
        LOG(INFO) << "User authenticated successfully";
        return;
      }
    } catch (...) {
    }
  }

  LOG(ERROR) << "Authentication failed";
  res.status = 403;
  res.set_content(
      fmt::format("Not authorized\n{}", response ? response->body : "null"),
      kTextPlain);
}

std::tuple<std::string, std::string, int> OsuClient::RefreshToken(
    std::string const& refresh_token) {
  httplib::Client cli(Osu::Host::kIdHost);
  std::ostringstream o;
  o << "client_id=" << client_id_ << "&client_secret=" << secret_
    << "&grant_type=refresh_token"
    << "&refresh_token=" << refresh_token;
  auto response = cli.Post(
      Osu::Endpoint::kGetToken, o.str(), kApplicationUrlEncode);
  if (response && response->status == 200) {
    try {
      auto json = nlohmann::json::parse(response->body);
      if (json.contains("access_token") && json["access_token"].is_string()) {
        LOG(INFO) << "Token refreshed using refresh token";
        user_.reset(new OsuUser(client_id_, json["access_token"]));
        return { json["access_token"], json["refresh_token"],
          json["expires_in"] };
      }
    } catch (...) {
    }
  }

  LOG(ERROR) << "Error refreshing token";
  return {};
}

std::string OsuClient::GetContent(int resource_id,
    const nlohmann::json& vars) const {
  util::Parser parser;
  auto contents = resource_.GetResourceById(resource_id);
  parser.Replace(contents, vars);
  return contents;
}

void OsuClient::Console(const httplib::Request& req,
    httplib::Response& res) {
  if (req.method == "GET") {
    int64_t user_id = user_->GetUserInfo()["id"];
    nlohmann::json const vars{ { "client_id", client_id_ },
      { "access_token", token_->GetAccessToken() }, { "user_id", user_id },
      { "host", Osu::Host::kApiHost }, { "port", port_ } };
    res.set_content(GetContent(resource::kConsoleHtml, vars), kTextHtml);
    return;
  }

  std::string url = req.get_param_value("url");
  std::string method = req.get_param_value("method");
  std::string body = req.get_param_value("body");

  auto host = [&url]() -> std::string {
    if (auto p = url.find('/', 8); p != std::string::npos)
      return url.substr(0, p);
    return "";
  }();
  auto path = [&url]() -> std::string {
    if (auto p = url.find('/', 8); p != std::string::npos)
      return url.substr(p);
    return "";
  }();

  if (host.empty() || path.empty() || method.empty()) {
    res.set_content("Bad request", kTextPlain);
    res.status = 400;
    return;
  }

  httplib::Client cli(host);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id_);
  headers.emplace(
      "Authorization", fmt::format("Bearer {}", token_->GetAccessToken()));

  std::string response;
  int status = 404;
  const auto extract_response = [&](auto&& res) {
    if (res) {
      status = res->status;
      if (status == 200)
        response = res->body;
    }
  };
  try {
    if (method == "GET") {
      extract_response(cli.Get(path, headers));
    } else if (method == "POST") {
      extract_response(cli.Post(path, headers, body, kApplicationUrlEncode));
    } else if (method == "PATCH") {
      extract_response(cli.Patch(path, headers, body, kApplicationUrlEncode));
    } else if (method == "PUT") {
      extract_response(cli.Put(path, headers, body, kApplicationUrlEncode));
    } else if (method == "DELETE") {
      extract_response(cli.Delete(path, headers, body, kApplicationUrlEncode));
    }
  } catch (...) {
  }

  res.set_content(response, kTextPlain);
  res.status = status;
}

void OsuClient::RegisterEndpoints() {
  server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("Hello!", kTextPlain);
  });

  server_.Get(
      "/authorize", [this](const httplib::Request& req,
                        httplib::Response& res) { Authorize(req, res); });

  server_.Get("/console", [this](const httplib::Request& req,
                              httplib::Response& res) { Console(req, res); });
  server_.Post("/console", [this](const httplib::Request& req,
                               httplib::Response& res) { Console(req, res); });
}

void OsuUser::GetOsuUserInfo(std::string const& client_id,
    std::string const& jwt) {
  httplib::Client cli(Osu::Host::kApiHost);
  httplib::Headers headers;
  headers.emplace("Content-type", kApplicationJson);
  headers.emplace("Accept", kApplicationJson);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt));
  auto res = cli.Get(Osu::Endpoint::kUserMe, headers);
  if (!res)
    return;

  try {
    user_info_ = nlohmann::json::parse(res->body);
  } catch (...) {
  }
}

OsuToken::OsuToken(std::string access_token,
    std::string refresh_token,
    int expires_in,
    refresh_function_t refresh_fun)
    : access_token_(access_token), refresh_token_(refresh_token) {
  refresher_ = std::async(
      std::launch::async, [this, expires_in, fun = std::move(refresh_fun)] {
        constexpr int kRefreshBeforeSecs = 120;
        auto exp = expires_in;
        do {
          std::unique_lock lock(mutex_);
          if (!cv_.wait_for(lock,
                  std::chrono::seconds(exp - kRefreshBeforeSecs),
                  [&] { return quit_; })) {
            LOG(INFO) << "Token is expiring soon; trying to refresh it";
            auto [access_token, refresh_token, new_expires_in] = fun(
                refresh_token_);
            if (access_token.empty()) {
              LOG(ERROR) << "Could not refresh token";
              return;
            }

            LOG(INFO) << "Token refreshed successfully";

            access_token_ = std::move(access_token);
            refresh_token_ = std::move(refresh_token);
            exp = new_expires_in;
          } else
            break;
        } while (!quit_);
      });
}

OsuToken::~OsuToken() {
  std::unique_lock lock(mutex_);
  quit_ = true;
  cv_.notify_one();
}
}  // namespace network
