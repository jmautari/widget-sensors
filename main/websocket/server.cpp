#include "websocket/server.hpp"
#include <cassert>

namespace network {
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

WebsocketServer::WebsocketServer(unsigned port) : port_(port) {
  assert(port_ > 0);

  m_server.init_asio();

  m_server.clear_access_channels(websocketpp::log::alevel::all);
  m_server.set_access_channels(websocketpp::log::elevel::info);

  // Register handler callbacks
  m_server.set_open_handler(bind(&WebsocketServer::OnOpen, this, _1));
  m_server.set_close_handler(bind(&WebsocketServer::OnClose, this, _1));
  m_server.set_message_handler(bind(&WebsocketServer::OnMessage, this, _1, _2));
}

WebsocketServer::~WebsocketServer() {
  runner_.join();
}

bool WebsocketServer::Start(message_handler_t on_message) {
  assert(on_message != nullptr);

  on_message_ = std::move(on_message);
  m_server.listen(port_);
  m_server.start_accept();

  runner_ = std::thread([&] {
    try {
      m_server.run();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  });
  return true;
}

bool WebsocketServer::Send(connection_hdl hdl, const char* data, size_t size) {
  try {
    m_server.send(hdl, data, size, websocketpp::frame::opcode::TEXT);
    return true;
  } catch (...) {
    std::cout << "Cannot send " << size << " bytes" << std::endl;
    return false;
  }
}

void WebsocketServer::Shutdown() {
  m_server.stop_listening();
  m_server.stop();
}

void WebsocketServer::OnOpen(connection_hdl hdl) {
  std::cout << "Client connection opened" << std::endl;
}

void WebsocketServer::OnClose(connection_hdl hdl) {
  std::cout << "Client connection closed" << std::endl;
}

void WebsocketServer::OnMessage(connection_hdl hdl, server::message_ptr msg) {
  on_message_(hdl, msg.get()->get_payload());
}
}  // namespace network
