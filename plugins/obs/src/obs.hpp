#pragma once
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace network {
typedef websocketpp::client<websocketpp::config::asio_client> client_t;
using websocketpp::connection_hdl;
using message_handler_t = std::function<void(connection_hdl hdl,
    const std::string&)>;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

class ObsWebClient {
public:
  ObsWebClient() = delete;

  ObsWebClient(std::string host, unsigned port, std::string password);
  ~ObsWebClient();

  bool Start(message_handler_t on_message = nullptr);
  bool Send(connection_hdl hdl, const char* data, size_t size);
  void Shutdown();

  bool HandleResponse(connection_hdl hdl, client_t::message_ptr msg);
  bool Request(std::string const& cmd,
      nlohmann::json const& params = nlohmann::json::object());

private:
  void OnOpen(connection_hdl hdl);
  void OnFail(connection_hdl hdl);
  void OnClose(connection_hdl hdl);
  void OnMessage(connection_hdl hdl, message_ptr msg);

  std::string GeneratePaswordHash(nlohmann::json const& payload) const;

  std::thread runner_;
  message_handler_t on_message_;
  std::string host_;
  unsigned port_{};
  std::string password_;
  client_t client_;
  int64_t request_id_{};
  connection_hdl server_;
};
}  // namespace network
