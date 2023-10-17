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
#include "websocket/server.hpp"
#include <cassert>

namespace network {
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

WebsocketServer::WebsocketServer(unsigned port) : port_(port) {
  assert(port_ > 0);

  server_.init_asio();

  server_.clear_access_channels(websocketpp::log::alevel::all);
  server_.set_access_channels(websocketpp::log::elevel::info);

  // Register handler callbacks
  server_.set_open_handler(bind(&WebsocketServer::OnOpen, this, _1));
  server_.set_close_handler(bind(&WebsocketServer::OnClose, this, _1));
  server_.set_message_handler(bind(&WebsocketServer::OnMessage, this, _1, _2));
}

WebsocketServer::~WebsocketServer() {
  runner_.join();
}

bool WebsocketServer::Start(message_handler_t on_message) {
  assert(on_message != nullptr);

  on_message_ = std::move(on_message);
  server_.listen(port_);
  server_.start_accept();

  runner_ = std::thread([&] {
    try {
      server_.run();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  });
  return true;
}

bool WebsocketServer::Send(connection_hdl hdl, const char* data, size_t size) {
  try {
    server_.send(hdl, data, size, websocketpp::frame::opcode::TEXT);
    return true;
  } catch (...) {
    std::cout << "Cannot send " << size << " bytes" << std::endl;
    return false;
  }
}

void WebsocketServer::Shutdown() {
  server_.stop_listening();
  server_.stop();
}

void WebsocketServer::OnOpen(connection_hdl hdl) {
  std::cout << "Client connection opened" << std::endl;
}

void WebsocketServer::OnClose(connection_hdl hdl) {
  std::cout << "Client connection closed" << std::endl;
}

void WebsocketServer::OnMessage(connection_hdl hdl, server_t::message_ptr msg) {
  if (on_message_)
    on_message_(hdl, msg.get()->get_payload());
}
}  // namespace network
