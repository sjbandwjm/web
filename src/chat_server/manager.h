#pragma once
#include "frontend.h"
#include "connection.h"
#include "room.h"
#include "spsc_executor.h"

namespace jiayou::chat {

class ChatServer {
 public:
  ChatServer();
  void InitAndRun(int port);
  void Join();

 private:
  //websocket callback
  void OnUpgrade_(Server::HttpResponse* res, Server::HttpRequest* req, const Server::HandleContextPtr& context);
  void OnOpen_(ConnectionHdl hdl);
  void OnClose_(ConnectionHdl hdl, int code, std::string_view msg);
  void OnMessage_(ConnectionHdl hdl, const std::string& msg, uWS::OpCode code);

  //perrooms
  struct RoomWorkerContext {
    void Reset() {};
  };

  static void PerRoomWorkerFunc(RoomWorkerContext&& context, URoomPtr& room);

 private:
  Server frontend_;
  std::atomic<int> connection_counter_{0};
  jiayou::base::KeyedSPSCWorker<std::string, RoomWorkerContext, URoomPtr> rooms_wokers_;
};

}
