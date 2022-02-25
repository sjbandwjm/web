#include "manager.h"
#include <butil/logging.h>
#include <gflags/gflags.h>

DEFINE_int32(listen_thread_cnt, 1, "listen thread count");
namespace jiayou::chat {

void ChatServer::PerRoomWorkerFunc(RoomWorkerContext&& context, URoomPtr& room) {}

ChatServer::ChatServer() {
  frontend_.SetWebsockHandler(Server::Handles{
    .on_close = std::bind(&ChatServer::OnClose_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    .on_open = std::bind(&ChatServer::OnOpen_, this, std::placeholders::_1),
    .on_message = std::bind(&ChatServer::OnMessage_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    .on_ping = [](auto) {},
    .on_pong = [](auto) {},
    .on_upgrade = std::bind(&ChatServer::OnUpgrade_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
  });
  rooms_wokers_.Init(&PerRoomWorkerFunc);
}


void ChatServer::InitAndRun(int port) {
  frontend_.InitAndRun(port, FLAGS_listen_thread_cnt);
}

void ChatServer::Join() {
  frontend_.Join();
}

void ChatServer::OnUpgrade_(Server::HttpResponse*, Server::HttpRequest*, const Server::HandleContextPtr& context) {
  context->data.connection_id = std::to_string(connection_counter_.fetch_add(1, std::memory_order::memory_order_relaxed));
}

void ChatServer::OnOpen_(ConnectionHdl hdl) {
  LOG(INFO) << "open conention:" << hdl.hdl.lock()->data.connection_id;
}

void ChatServer::OnClose_(ConnectionHdl hdl, int, std::string_view) {
  LOG(INFO) << "close conention:" << hdl.hdl.lock()->data.connection_id;
}

void ChatServer::OnMessage_(ConnectionHdl hdl, const std::string& msg, uWS::OpCode ) {
  LOG(INFO) << "conention_id:" << hdl.hdl.lock()->data.connection_id
            << " message:" << msg;
}


}