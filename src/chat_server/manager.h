#pragma once
#include "connection.h"
#include "room.h"
#include "spsc_executor.h"
#include "common/rapidjson_wrapper.h"

namespace jiayou::chat {

class ChatServer {
 public:
  ChatServer();
  void InitAndRun(int port);
  void Join();

 private:
  typedef std::function<void(ConnectionCtx, const rapidjson::Document&)> MessageProcessFunc;
  //websocket callback
  void OnUpgrade_(Server::HttpResponse* res, Server::HttpRequest* req, const Server::HandleContextPtr& context);
  void OnOpen_(ConnectionCtx hdl);
  void OnClose_(ConnectionCtx hdl, int code, std::string_view msg);
  void OnMessage_(ConnectionCtx hdl, const std::string& msg, uWS::OpCode code);

  // per rooms
  static void PerRoomWorkerFunc(RoomWorkerContext&& context, RoomPtr& room);
  // check rooms
  void PerRoomCheck_(const RoomPtr& room);

  // client signal
 private:
  void LogIn_(ConnectionCtx, const rapidjson::Document&);
  void LogOut_(ConnectionCtx, const rapidjson::Document&);
  void EnterRoom_(ConnectionCtx, const rapidjson::Document&);
  void CreateRoom_(ConnectionCtx, const rapidjson::Document&);
  void SendMsg_(ConnectionCtx, const rapidjson::Document&);

 private:
  Server frontend_;
  std::atomic<int> connection_counter_{0};
  jiayou::base::KeyedSPSCWorker<std::string, RoomWorkerContext, RoomPtr> rooms_wokers_;

  std::mutex rooms_mu_;
  std::map<std::string, RoomPtr> rooms_;

  std::unordered_map<std::string, MessageProcessFunc> message_processor_;
};

}
