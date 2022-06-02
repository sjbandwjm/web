#include "manager.h"
#include <sstream>
#include <fstream>
#include <butil/logging.h>
#include <gflags/gflags.h>
#include "common/rapidjson_wrapper.h"
#include "HttpResponse.h"
#include "HttpParser.h"
#include "utils/util.h"
#include "common/timer_thread.h"
#include "common/wait_group.h"


DEFINE_string(ssl_key_path, "", "key.pem path");
DEFINE_string(ssl_cert_path, "", "cert.pem path");
DEFINE_string(ssl_passphrase, "", "ssl passphrase");
DEFINE_int32(listen_thread_cnt, 1, "listen thread count");
DEFINE_int32(clear_room_loop_interval_s, 10, "clear rooms interval of check");
namespace jiayou::chat {

void ChatServer::PerRoomWorkerFunc(RoomWorkerContext&& context, RoomPtr& room) {
  if (!room) {
    LOG(ERROR) << "room ptr invalid room_id:" << context.room_id;
    return;
  }

  if (context.fn) {
    context.fn(room, &context);
  }
}

void ChatServer::PerRoomCheck_(const RoomPtr& room) {
  common::WaitGroupPtr done = std::make_shared<common::WaitGroup>();

  RoomWorkerContext c;
  c.fn = [=](const RoomPtr& p, RoomWorkerContext*) {
    p->CheckStatus();
    done->Done();
    LOG(INFO) << "room check room_id:" << p->room_id()
              << " user_num : " << p->users().size()
              << " status: " << (int)p->GetStatus();
  };

  done->Add(1);
  rooms_wokers_.Post(room->room_id(), std::move(c));
  done->BlockWait();

  int32_t next_timer_interval_s = FLAGS_clear_room_loop_interval_s;

  switch (room->GetStatus()) {
    case Room::Status::kWaitDel: {
      next_timer_interval_s = 5;
      LOG(INFO) << "room will be deleted room_id:" << room->room_id()
                  << " after seconds:" << next_timer_interval_s;
      break;
    }
    case Room::Status::kDeleted: {
      std::lock_guard lk(rooms_mu_);
      rooms_.erase(room->room_id());
      rooms_wokers_.Stop(room->room_id(), [](auto& room){
        LOG(INFO) << "room erase, room_id:" << room->room_id();
      });
      return;
    }
    default:
      break;
  }

  // todo next timer schedule
  jiayou::common::GetGlobalTimer()->Schedule([this, room](){
    PerRoomCheck_(room);
  }, jiayou::common::SteadyTimeGen<std::chrono::milliseconds>(next_timer_interval_s * 1000));
}


ChatServer::ChatServer() {
  frontend_.SetWebsockHandler(Server::Handles{
    .on_close = std::bind(&ChatServer::OnClose_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    .on_open = std::bind(&ChatServer::OnOpen_, this, std::placeholders::_1),
    .on_message = std::bind(&ChatServer::OnMessage_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    .on_ping = [](auto) {},
    .on_pong = [](auto) {},
    .on_upgrade = std::bind(&ChatServer::OnUpgrade_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
  });

  message_processor_.emplace("login", [this](ConnectionCtx ctx, const rapidjson::Document& doc) { LogIn_(ctx, doc); });
  message_processor_.emplace("enter_room", [this](ConnectionCtx ctx, const rapidjson::Document& doc) { EnterRoom_(ctx, doc); });
  message_processor_.emplace("create_room", [this](ConnectionCtx ctx, const rapidjson::Document& doc) { CreateRoom_(ctx, doc); });
  message_processor_.emplace("send_message", [this](ConnectionCtx ctx, const rapidjson::Document& doc) { SendMsg_(ctx, doc); });
  message_processor_.emplace("logout", [this](ConnectionCtx ctx, const rapidjson::Document& doc) { LogOut_(ctx, doc); });
}


void ChatServer::InitAndRun(int port) {
  rooms_wokers_.Init(&PerRoomWorkerFunc);

  std::string html_root = jiayou::util::AbsolutePath("resource").append("/");

  frontend_.AddHttpHandler("/", [html_root](Server::HttpResponse* resp, auto*){
    std::ifstream ifs(html_root + "index.html");
    std::string data((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));
    resp->end(data);
  });

  frontend_.AddHttpHandler("/*", [html_root](Server::HttpResponse* resp, uWS::HttpRequest* req){
    LOG(INFO) << "recv http req:" << req->getUrl();
    std::string path(html_root);
    path.append(req->getUrl());
    std::ifstream ifs(path);
    std::string data;
    if (ifs.good()) {
      data.assign((std::istreambuf_iterator<char>(ifs)),
                    (std::istreambuf_iterator<char>()));
    } else {
      resp->writeStatus("404 Not found");
      data = "file not found";
    }
    resp->end(data);
  });

  frontend_.InitSSL(FLAGS_ssl_key_path, FLAGS_ssl_cert_path, FLAGS_ssl_passphrase);
  frontend_.InitAndRun(port, FLAGS_listen_thread_cnt);
}

void ChatServer::Join() {
  frontend_.Join();
}

void ChatServer::OnUpgrade_(Server::HttpResponse*, Server::HttpRequest*, const Server::HandleContextPtr& context) {
  context->data.connection_id = std::to_string(connection_counter_.fetch_add(1, std::memory_order::memory_order_relaxed));
}

void ChatServer::OnOpen_(ConnectionCtx ctx) {
  auto hdl = ctx.hdl.lock();
  LOG(INFO) << "open conention:" << hdl->data.connection_id;
}

void ChatServer::OnClose_(ConnectionCtx ctx, int, std::string_view) {
  LOG(INFO) << "close conention:" << ctx.hdl.lock()->data.connection_id;
  LogOut_(ctx, {});
}

void ChatServer::OnMessage_(ConnectionCtx ctx, const std::string& msg, uWS::OpCode ) {
  LOG(INFO) << "on_message conention_id:" << ctx.hdl.lock()->data.connection_id
            << " message:" << msg;
  auto hdl = ctx.hdl.lock();

  rapidjson::Document doc;
  if (doc.Parse(msg.c_str()).HasParseError() || !doc.IsObject()) {
    LOG(WARNING) << "recv invalid json msg"
                  << " connection_id:" << hdl->data.connection_id
                  << " user_name:" << hdl->data.user_name
                  << " error:" << doc.GetParseError()
                  << " msg:" << msg;
    return;
  }

  ::jiayou::base::RapidJsonWrapper wrapper(doc);
  auto it = wrapper.FindMemberString("cmd");
  if (!it) {
    LOG(WARNING) << "recv msg dosent have cmd key "
                  << " connection_id:" << hdl->data.connection_id
                  << " user_name:" << hdl->data.user_name
                  << " msg: " << msg;
    return;
  }

  auto p = message_processor_.find(*it);
  if (p == message_processor_.end()) {
    LOG(WARNING) << "recv msg have invalid cmd"
                  << " connection_id:" << hdl->data.connection_id
                  << " user_name:" << hdl->data.user_name
                  << " cmd:"  << *it;
    return;
  }

  p->second(ctx, doc);
  // hdl.hdl.lock()->ws->send(msg);
}

void ChatServer::LogIn_(ConnectionCtx ctx, const rapidjson::Document& doc) {
  LOG(INFO) << "process login" << ctx.hdl.lock()->data.connection_id;
  auto hdl = ctx.hdl.lock();

  if (hdl->data.is_login) {
    return;
  }

  base::RapidJsonWrapper w(doc);

  std::string name;
  if (auto it = w.FindMemberString("user_name"); it) {
    int len = strnlen(*it, 256);
    if (len == 0) {
      name.append("unset");
    } else {
      name.assign(*it, len);
    }
  } else {
    name.append("unset");
  }

  hdl->data.user_name = name;
  hdl->data.user_id = name.append("_").append(hdl->data.connection_id);

  hdl->data.is_login = true;

  base::JsonStringWriter res;
  res["cmd"] = "on_login";
  res["user_id"] = hdl->data.user_id;
  res["user_name"] = hdl->data.user_name;
  res["cid"] = hdl->data.connection_id;
  Server::Send(ctx.hdl, res.GetStringView());
  LOG(INFO) << "login success user:" << hdl->data.user_name;
}

void ChatServer::LogOut_(ConnectionCtx ctx, const rapidjson::Document&) {
  auto hdl = ctx.hdl.lock();
  if (!hdl->data.is_login) {
    return;
  }

  hdl->data.is_login = false;
  // TODO: logout with spec rooms
  std::lock_guard lk(rooms_mu_);
  for (auto& r : rooms_) {
    RoomWorkerContext wctx;
    wctx.from_user_id = hdl->data.user_id;

    wctx.fn = [](const RoomPtr& p, RoomWorkerContext* room_wctx){
      if (!p->IsUserEnter(room_wctx->from_user_id)) {
        return;
      }
      p->DelUser(*room_wctx);
      base::JsonStringWriter res;
      res["cmd"] = "on_user_quit";
      res["from_user_id"] = room_wctx->from_user_id;
      res["from_room_id"] = p->room_id();
      p->SendToAll(res.GetStringView());
    };
    rooms_wokers_.Post(r.first, std::move(wctx));
  }

}

void ChatServer::EnterRoom_(ConnectionCtx ctx, const rapidjson::Document& doc) {
  auto hdl = ctx.hdl.lock();

  if (!hdl->data.is_login) {
    Server::Send(ctx.hdl, std::string_view("please login first"));
    LOG(WARNING) << "user is not login user:" << hdl->data.user_name
                  << " cid:" << hdl->data.connection_id;
    return;
  }

  base::RapidJsonWrapper w(doc);
  auto room_id = w.FindMemberString("room_id");
  if (!room_id) {
    LOG(WARNING) << "invalid key room_id user:" << hdl->data.user_name;
    return;
  }

  {
    std::lock_guard lk(rooms_mu_);
    if (auto it = rooms_.find(*room_id); it == rooms_.end() || it->second->GetStatus() != Room::Status::kNormal) {
      Server::Send(ctx.hdl, "no such room");
      return;
    }
  }

  std::string passwd;
  if (auto passward_it = w.FindMemberString("passwd"); passward_it) {
    passwd = *passward_it;
  }

  RoomWorkerContext room_ctx;
  room_ctx.room_id = *room_id;
  room_ctx.passwd = passwd;
  room_ctx.from_user_id = hdl->data.user_id;
  room_ctx.ctx = ctx;
  room_ctx.fn = [](const RoomPtr& p, RoomWorkerContext* room_wctx){
    if (p->IsUserEnter(room_wctx->from_user_id) || !p->AddUser(*room_wctx)) {
      Server::Send(room_wctx->ctx.hdl, "enter room fail");
      return;
    }
    base::JsonStringWriter res;
    res["cmd"] = "on_user_enter";
    res["from_room_id"] = room_wctx->room_id;
    res["from_user_id"] = room_wctx->from_user_id;
    p->SendToAll(res.GetStringView());

    base::JsonStringWriter res1;
    res1["cmd"] = "enter_room_success";
    res1["from_room_id"] = room_wctx->room_id;
    Server::Send(room_wctx->ctx.hdl, res1.GetStringView());

    p->SendUserList(room_wctx->ctx.hdl);
  };

  rooms_wokers_.Post(*room_id, std::move(room_ctx));
}

void ChatServer::CreateRoom_(ConnectionCtx ctx, const rapidjson::Document& doc) {
  auto hdl = ctx.hdl.lock();

  if (!hdl->data.is_login) {
    Server::Send(ctx.hdl, "please login first");
    return;
  }

  base::RapidJsonWrapper w(doc);

  // check_token
  auto token = w.FindMemberString("token");
  if (!token || strncmp(*token, "liershizhu", 10)) {
    Server::Send(ctx.hdl, "invalid token");
    LOG(WARNING) << " invalid token, create room fail";
    return;
  }

  auto room_it = w.FindMemberString("room_id");
  if (!room_it) {
    LOG(WARNING) << "not room_id, user:" << hdl->data.user_name;
    return;
  }
  std::string room_id;
  if (strnlen(*room_it, 65) > 64) {
    Server::Send(ctx.hdl, "invalid room_id");
    return;
  }
  room_id = *room_it;
  RoomPtr roomptr;
  {
    std::lock_guard lk(rooms_mu_);
    auto& r = rooms_[room_id];
    if (!r) {
      r = std::make_shared<Room>();
      roomptr = r;
    }
  }

  if (!roomptr) {
    LOG(WARNING) << "exist room:" << room_id
                  << " user:" << hdl->data.user_name;
    Server::Send(ctx.hdl, "exist room");
    return;
  }

  std::string passwd;
  if (auto it = w.FindMemberString("passwd"); it) {
    if (strnlen(*it, 17) <= 16) {
      passwd = *it;
    }
  }

  rooms_wokers_.SetLocalStorage(room_id, std::move(roomptr));

  RoomWorkerContext room_ctx;
  room_ctx.room_id = room_id;
  room_ctx.passwd = std::move(passwd);
  room_ctx.from_user_id = hdl->data.user_id;
  room_ctx.ctx = ctx;
  room_ctx.fn = [this](const RoomPtr& p, RoomWorkerContext* room_wctx){
    p->Init(*room_wctx);
    if(!p->AddUser(*room_wctx)) {
      Server::Send(room_wctx->ctx.hdl, "enter room failed");
      return;
    }
    base::JsonStringWriter res;
    res["cmd"] = "enter_room_success";
    res["from_room_id"] = room_wctx->room_id;
    Server::Send(room_wctx->ctx.hdl, res.GetStringView());
    p->SendUserList(room_wctx->ctx.hdl);

    // timer to clear this room
    jiayou::common::GetGlobalTimer()->Schedule([this, room=p](){
      room->SetStatus(Room::Status::kNormal);
      PerRoomCheck_(room);
    }, jiayou::common::SteadyTimeGen<std::chrono::milliseconds>(FLAGS_clear_room_loop_interval_s * 1000));
  };


  LOG(INFO) << "create room success, user:" << hdl->data.user_name
              << " room_id:" << room_ctx.room_id
              << " passwd:" << room_ctx.passwd;
  rooms_wokers_.Post(room_id, std::move(room_ctx));

}

void ChatServer::SendMsg_(ConnectionCtx ctx, const rapidjson::Document& doc) {
  auto hdl = ctx.hdl.lock();

  if (!hdl->data.is_login) {
    Server::Send(ctx.hdl, "please login first");
    return;
  }

  base::RapidJsonWrapper w(doc);
  auto to_room_id = w.FindMemberString("to_room_id");
  if (!to_room_id) {
    return;
  }

  std::string message;
  auto it = w.FindMemberString("message");
  if (!it) {
    return;
  }
  message = *it;

  {
    std::lock_guard lk(rooms_mu_);
    if (auto it = rooms_.find(*to_room_id); it == rooms_.end()) {
      return;
    }
  }

  RoomWorkerContext wctx;
  wctx.ctx = ctx;
  wctx.from_name = hdl->data.user_name;
  wctx.from_user_id = hdl->data.user_id;
  wctx.room_id = *to_room_id;
  wctx.fn = [msg = std::move(message)](const RoomPtr& p, RoomWorkerContext* room_wctx){
    if (!p->IsUserEnter(room_wctx->from_user_id)) {
      Server::Send(room_wctx->ctx.hdl, "send fail");
      return;
    }
    base::JsonStringWriter res;
    res["message"] = std::move(msg);
    res["from_user"] = room_wctx->from_user_id;
    res["from_room"] = room_wctx->room_id;
    res["timestamp"] = butil::gettimeofday_ms();
    res["cmd"] = "recv_message";

    p->SendToAll(res.GetStringView());
  };

  rooms_wokers_.Post(*to_room_id, std::move(wctx));
}

}