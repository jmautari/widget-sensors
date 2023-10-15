#include "twitch.hpp"
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

struct Twitch {
  struct Host {
    static constexpr char kIdHost[] = "https://id.twitch.tv";
    static constexpr char kApiHost[] = "https://api.twitch.tv";
  };
  struct Endpoint {
    static constexpr char kAuth[] = "/oauth2/authorize";
    static constexpr char kGetToken[] = "/oauth2/token";
    static constexpr char kUsers[] = "/helix/users";
    static constexpr char kGames[] = "/helix/games";
    static constexpr char kChannels[] = "/helix/channels";
  };
  static constexpr char kScope[] = "channel%3Amanage%3Abroadcast";
};

constexpr char kTextPlain[] = "text/plain";
constexpr char kTextHtml[] = "text/html";
constexpr char kApplicationUrlEncode[] = "application/x-www-form-urlencoded";

TwitchClient::TwitchClient(std::filesystem::path data_dir)
    : data_dir_(data_dir), resource_{ hInstance } {
  ADD_COMMAND(OpenConsole);
  ADD_COMMAND(OpenFile);
}

TwitchClient::~TwitchClient() {
  OutputDebugStringA(__FUNCTION__);
  Stop();
}

void TwitchClient::Stop() {
  if (server_.is_running())
    server_.stop();

  if (runner_.joinable())
    runner_.join();
}

void TwitchClient::StartListen(std::string client_id,
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
      OutputDebugStringA("Cannot start listening");
  });
}

std::string TwitchClient::GetAuthenticationUrl() {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int> dist(0, 99);
  std::ostringstream s;
  for (int i = 0; i < 16; i++)
    s << dist(mt);

  state_ = fmt::format("{}", std::hash<std::string>{}(s.str()));
  return fmt::format(
      "{}{}?response_type=code&client_id={}&redirect_uri={}&scope={}&state={}",
      Twitch::Host::kIdHost, Twitch::Endpoint::kAuth, client_id_, GetRedirUrl(),
      Twitch::kScope, state_);
}

nlohmann::json TwitchClient::GetGameInfo(const std::string& game_name) {
  auto json = game_.GetGameInfo(
      game_name, client_id_, token_->GetAccessToken());
  return json;
}

bool TwitchClient::Request(std::string const& cmd,
    nlohmann::json const& params) {
  try {
    if (auto fn = commands_.find(cmd); fn != commands_.end())
      fn->second(params);

    return true;
  } catch (...) {
    OutputDebugStringW(L"Error processing request!");
  }
  return false;
}

void TwitchClient::OpenConsole(nlohmann::json const& params) {
  static_cast<void>(util::OpenViaShell(
      L"http://localhost:" + std::to_wstring(port_) + L"/console"));
}

void TwitchClient::OpenFile(nlohmann::json const& params) {
  std::filesystem::path file;
  try {
    std::error_code ec;
    file = data_dir_ / params["file"];
    if (std::filesystem::exists(file, ec)) {
      static_cast<void>(util::OpenViaShell(L"file:///" + file.wstring()));
    } else {
      OutputDebugStringW(
          (L"File " + file.wstring() + L" does not exist").c_str());
    }
  } catch (...) {
    OutputDebugStringW((L"Error opening file " + file.wstring()).c_str());
  }
}

void TwitchClient::Authorize(const httplib::Request& req,
    httplib::Response& res) {
  /*
    code=gulfwdmys5lsm6qyz4xiz9q32l10
    &scope=channel%3Amanage%3Apolls+channel%3Aread%3Apolls
    &state=c3ab8aa609ea11e793ae92361f002671
  */
  std::string code = req.get_param_value("code");
  std::string scope = req.get_param_value("scope");
  std::string state = req.get_param_value("state");

  if (code.empty() || scope.empty() || state != state_) {
    res.set_content("Access denied or invalid response", kTextPlain);
    return;
  }

  ExchangeCodeWithToken(code, res);
}

void TwitchClient::ExchangeCodeWithToken(std::string const& code,
    httplib::Response& res) {
  /*
    client_id=hof5gwx0su6owfnys0yan9c87zr6t
    &client_secret=41vpdji4e9gif29md0ouet6fktd2
    &code=gulfwdmys5lsm6qyz4xiz9q32l10
    &grant_type=authorization_code
    &redirect_uri=http://localhost:3000
  */
  httplib::Client cli(Twitch::Host::kIdHost);
  std::ostringstream o;
  o << "client_id=" << client_id_ << "&client_secret=" << secret_
    << "&code=" << code << "&grant_type=authorization_code"
    << "&redirect_uri=http://localhost:" << port_ << "/authorize";
  auto response = cli.Post(
      Twitch::Endpoint::kGetToken, o.str(), kApplicationUrlEncode);

  if (response && response->status == 200) {
    try {
      auto json = nlohmann::json::parse(response->body);
      if (json.contains("access_token") && json["access_token"].is_string()) {
        token_ = std::make_unique<TwitchToken>(json["access_token"],
            json["refresh_token"], json["expires_in"],
            [&](auto refresh_token) { return RefreshToken(refresh_token); });
        user_ = std::make_unique<TwitchUser>(
            client_id_, token_->GetAccessToken());

        res.set_content("You can close this tab now", kTextPlain);
        res.status = 200;
        return;
      }
    } catch (...) {
    }
  }

  res.status = 403;
  res.set_content(
      fmt::format("Not authorized\n{}", response ? response->body : "null"),
      kTextPlain);
}

std::tuple<std::string, std::string, int> TwitchClient::RefreshToken(
    std::string const& refresh_token) {
  httplib::Client cli(Twitch::Host::kIdHost);
  std::ostringstream o;
  o << "client_id=" << client_id_ << "&client_secret=" << secret_
    << "&grant_type=refresh_token"
    << "&refresh_token=" << refresh_token;
  auto response = cli.Post(
      Twitch::Endpoint::kGetToken, o.str(), kApplicationUrlEncode);
  if (response && response->status == 200) {
    try {
      auto json = nlohmann::json::parse(response->body);
      if (json.contains("access_token") && json["access_token"].is_string()) {
        user_.reset(new TwitchUser(client_id_, json["access_token"]));
        return { json["access_token"], json["refresh_token"],
          json["expires_in"] };
      }
    } catch (...) {
    }
  }

  return {};
}

std::string TwitchClient::GetContent(int resource_id,
    const nlohmann::json& vars) const {
  util::Parser parser;
  auto contents = resource_.GetResourceById(resource_id);
  parser.Replace(contents, vars);
  return contents;
}

void TwitchClient::Console(const httplib::Request& req,
    httplib::Response& res) {
  if (req.method == "GET") {
    std::string user_id = user_->GetUserInfo()["data"][0]["id"];
    nlohmann::json const vars{ { "client_id", client_id_ },
      { "access_token", token_->GetAccessToken() },
      { "user_id", std::move(user_id) }, { "host", Twitch::Host::kApiHost },
      { "port", port_ } };
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

void TwitchClient::RegisterEndpoints() {
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

void TwitchClient::SetBroadcastInfo(const std::string& game_id,
    const std::string& title) const {
  auto user = user_->GetUserInfo();
  if (!user.contains("data") || !user["data"].is_array())
    return;

  std::string const user_id = user["data"][0]["id"];
  std::ostringstream s;
  s << "game_id=" << game_id;
  if (!title.empty())
    s << "&title=" << title;

  httplib::Client cli(Twitch::Host::kApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id_);
  headers.emplace(
      "Authorization", fmt::format("Bearer {}", token_->GetAccessToken()));
  try {
    auto res = cli.Patch(fmt::format("{}?broadcaster_id={}",
                             Twitch::Endpoint::kChannels, user_id),
        headers, s.str(), kApplicationUrlEncode);
    if (!res)
      OutputDebugStringA("Can't set broadcast info");
    else
      OutputDebugStringA("Broadcast info set successfully");
  } catch (...) {
  }
}

void TwitchUser::GetTwitchUserInfo(std::string const& client_id,
    std::string const& jwt) {
  httplib::Client cli(Twitch::Host::kApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt));
  auto res = cli.Get(Twitch::Endpoint::kUsers, headers);
  if (!res)
    return;

  try {
    user_info_ = nlohmann::json::parse(res->body);
  } catch (...) {
  }
}

nlohmann::json TwitchGame::GetGameInfo(std::string const& game_name,
    std::string const& client_id,
    std::string const& jwt) {
  if (auto it = games_.find(game_name); it != games_.end()) {
    OutputDebugStringA(("Returning cached response for " + game_name).c_str());
    return it->second;
  }

  httplib::Client cli(Twitch::Host::kApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt));

  httplib::Params params;
  params.emplace("name", game_name);
  auto res = cli.Get(Twitch::Endpoint::kGames, params, headers);
  if (!res) {
    OutputDebugStringA(("Could not find data for " + game_name).c_str());
    return {};
  }

  try {
    auto json = nlohmann::json::parse(res->body);
    OutputDebugStringA(json.dump().c_str());
    games_.emplace(game_name, json);
    return json;
  } catch (...) {
    OutputDebugStringA(("Error parsing JSON " + res->body).c_str());
  }
  return {};
}

TwitchToken::TwitchToken(std::string access_token,
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
            OutputDebugStringA("Token is expiring soon; trying to refresh it");
            auto [access_token, refresh_token, new_expires_in] = fun(
                refresh_token_);
            if (access_token.empty()) {
              OutputDebugStringA("Could not refresh token");
              return;
            }

            OutputDebugStringA("Token refreshed successfully");

            access_token_ = std::move(access_token);
            refresh_token_ = std::move(refresh_token);
            exp = new_expires_in;
          } else
            break;
        } while (!quit_);
      });
}

TwitchToken::~TwitchToken() {
  std::unique_lock lock(mutex_);
  quit_ = true;
  cv_.notify_one();
}
}  // namespace network
