#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <functional>
#include <vector>
#include <thread>
#include <string>

namespace network {
typedef websocketpp::server<websocketpp::config::asio> server_t;
using websocketpp::connection_hdl;
using message_handler_t = std::function<void(connection_hdl hdl,
    const std::string&)>;

class WebsocketServer {
public:
  WebsocketServer() = delete;

  explicit WebsocketServer(unsigned port);
  ~WebsocketServer();

  bool Start(message_handler_t on_message);
  bool Send(connection_hdl hdl, const char* data, size_t size);
  void Shutdown();

private:
  void OnOpen(connection_hdl hdl);
  void OnClose(connection_hdl hdl);
  void OnMessage(connection_hdl hdl, server_t::message_ptr msg);

  std::thread runner_;
  message_handler_t on_message_;
  unsigned port_{};
  server_t server_;
};
}  // namespace network