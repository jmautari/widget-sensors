#include "twitch.hpp"
#include "fmt/format.h"
#include <random>
#include <sstream>

namespace network {
constexpr char kTwitchIdHost[]{ "https://id.twitch.tv" };
constexpr char kTwitchApiHost[]{ "https://api.twitch.tv" };
constexpr char kTwitchAuthEndpoint[]{ "/oauth2/authorize" };
constexpr char kTwitchGetTokenEndpoint[]{ "/oauth2/token" };
constexpr char kTwitchScope[]{ "channel%3Amanage%3Abroadcast" };
constexpr char kTwitchUserEndpoint[]{ "/helix/users" };
constexpr char kTwitchGamesEndpoint[]{ "/helix/games" };
constexpr char kTwitchChannelsEndpoint[]{ "/helix/channels" };

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
      kTwitchIdHost, kTwitchAuthEndpoint, client_id_, GetRedirUrl(),
      kTwitchScope, state_);
}

nlohmann::json TwitchClient::GetGameInfo(const std::string& game_name) {
  auto json = game_.GetGameInfo(game_name, client_id_, jwt_);
  return json;
}

bool TwitchClient::Request(std::string const& cmd,
    nlohmann::json const& params) {
  try {
    nlohmann::json j;
    return true;
  } catch (...) {
    OutputDebugStringW(L"Error sending request to server!");
  }
  return false;
}

void TwitchClient::Authorize(const httplib::Request& req,
    httplib::Response& res) {
  OutputDebugStringA(__FUNCTION__);
  /*
    code=gulfwdmys5lsm6qyz4xiz9q32l10
    &scope=channel%3Amanage%3Apolls+channel%3Aread%3Apolls
    &state=c3ab8aa609ea11e793ae92361f002671
  */
  std::string code = req.get_param_value("code");
  std::string scope = req.get_param_value("scope");
  std::string state = req.get_param_value("state");

  if (code.empty() || scope.empty() || state != state_) {
    res.set_content("Access denied or invalid response", "text/plain");
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
  httplib::Client cli(kTwitchIdHost);
  std::ostringstream o;
  o << "client_id=" << client_id_;
  o << "&client_secret=" << secret_;
  o << "&code=" << code;
  o << "&grant_type=authorization_code";
  o << "&redirect_uri=" << fmt::format("http://localhost:{}/authorize", port_);
  auto response = cli.Post(
      kTwitchGetTokenEndpoint, o.str(), "application/x-www-form-urlencoded");

  if (response && response->status == 200) {
    try {
      auto json = nlohmann::json::parse(response->body);
      if (json.contains("access_token") && json["access_token"].is_string()) {
        jwt_ = json["access_token"];
        res.set_content("You can close this tab now", "text/plain");
        res.status = 200;

        user_ = std::make_unique<TwitchUser>(client_id_, jwt_);

        OutputDebugStringA(jwt_.c_str());
        return;
      }
    } catch (...) {
    }
  }

  res.status = 403;
  res.set_content(
      fmt::format("Not authorized\n{}", response ? response->body : "null"),
      "text/plain");
}

void TwitchClient::RegisterEndpoints() {
  server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("Hello!", "text/plain");
  });

  server_.Get(
      "/authorize", [this](const httplib::Request& req,
                        httplib::Response& res) { Authorize(req, res); });
}

void TwitchClient::SetBroadcastInfo(const std::string& game_id,
    const std::string& title) {
  auto user = user_->GetUserInfo();
  if (!user.contains("data") || !user["data"].is_array())
    return;

  std::string const user_id = user["data"][0]["id"];
  std::ostringstream s;
  s << "game_id=" << game_id;
  if (!title.empty())
    s << "&title=" << title;

  httplib::Client cli(kTwitchApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id_);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt_));
  try {

  auto res = cli.Patch(
      fmt::format("{}?broadcaster_id={}", kTwitchChannelsEndpoint, user_id),
      headers, s.str(), "application/x-www-form-urlencoded");
  if (!res)
    OutputDebugStringA("Can't set broadcast info");
  else
    OutputDebugStringA("Broadcast info set successfully");
  } catch (...) {
  }
}

void TwitchUser::GetTwitchUserInfo(std::string const& client_id,
    std::string const& jwt) {
  httplib::Client cli(kTwitchApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt));
  auto res = cli.Get(kTwitchUserEndpoint, headers);
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

  httplib::Client cli(kTwitchApiHost);
  httplib::Headers headers;
  headers.emplace("Client-Id", client_id);
  headers.emplace("Authorization", fmt::format("Bearer {}", jwt));

  httplib::Params params;
  params.emplace("name", game_name);
  auto res = cli.Get(kTwitchGamesEndpoint, params, headers);
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
}  // namespace network
