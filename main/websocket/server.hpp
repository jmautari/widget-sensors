/**
 * Widget Sensors
 * WebSocket Server
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