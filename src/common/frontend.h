#pragma once
#include <memory>
#include <atomic>
#include <bits/stdint-intn.h>
#include <string>
#include <thread>
#include <condition_variable>
#include <memory>
#include <map>
#include <functional>
#include <cstdlib>
#include <App.h>
#include <Loop.h>

#include <gflags/gflags.h>

#include "WebSocketProtocol.h"
#include "butil/strings/string_number_conversions.h"
#include "butil/logging.h"
#include "butil/time.h"
#include "bvar/passive_status.h"
#include "bvar/bvar.h"

namespace lightning::websocket {

DECLARE_double(ws_global_cps_limit_capacity);
DECLARE_double(ws_global_cps_limit_rate_s);
DECLARE_int32(ws_connection_count_limit);
DECLARE_double(ws_connection_uplink_qps_limit_capacity);
DECLARE_double(ws_connection_uplink_qps_limit_rate_s);

inline int64_t GetCurrentTimeMs() {
  return std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now()).time_since_epoch().count();
}

template <typename T, bool SSL>
class Server {
 public:
  struct HandleContext {
    Server* server;
    uWS::Loop* loop;
    uWS::WebSocket<SSL, true>* ws;
    std::string ip;
    std::string ua;

    T data;
  };

  struct PerSocketData {
    std::shared_ptr<HandleContext> handle_context;
    bool opened{false};
    // base::RateLimiter limiter;

    LTN_COUNT_OBJECT(PerSocketData);
  };

  typedef std::weak_ptr<HandleContext> ConnectionHdl;

  struct Context {
    int64_t cpuwide_time_us;
    ConnectionHdl hdl;

    void Reset() {
      hdl.reset();
    }
  };

  typedef uWS::TemplatedApp<SSL> UWSServer;
  typedef uWS::HttpResponse<SSL> HttpResponse;
  typedef std::function<void(uWS::HttpResponse<SSL>*, uWS::HttpRequest*, const std::shared_ptr<HandleContext>&)> UpgradeHandler;
  typedef std::function<void(Context, const std::string&, uWS::OpCode)> MessageHandler;
  typedef std::function<void(Context)> OpenHandler;
  typedef std::function<void(Context, int, std::string_view)> CloseHandler;
  typedef std::function<void(Context)> PingHandler;
  typedef std::function<void(Context)> PongHandler;
  typedef std::shared_ptr<Server> EndpontPtr;
  typedef uint64_t FinishFlagType;
  static const int kMaxThreadCnt = (sizeof(FinishFlagType) * 8);

  struct Handles{
    CloseHandler on_close;
    OpenHandler on_open;
    MessageHandler on_message;
    PingHandler on_ping;
    PongHandler on_pong;
    UpgradeHandler on_upgrade;
  };

  static void Send(ConnectionHdl hdl, std::string&& msg, uWS::OpCode op = uWS::TEXT, bool compress = false) {
    auto time = butil::cpuwide_time_us();
    if (auto conn = hdl.lock()) {
      conn->loop->defer([=](){
        if (auto conn = hdl.lock()) {
          if (conn->ws != nullptr) {
            conn->ws->send(msg, op, compress);
            conn->server->ws_send << butil::cpuwide_time_us() - time;
          }
        }
      });
    }
  }

  static void Send(ConnectionHdl hdl, std::string_view msg, uWS::OpCode op = uWS::TEXT, bool compress = false) {
    Send(hdl, std::move(std::string(msg.data(), msg.length())), op, compress);
  }

  static void Close(ConnectionHdl hdl) {
    if (auto conn = hdl.lock()) {
      conn->loop->defer([=](){
        if (auto conn = hdl.lock()) {
          if (conn->ws != nullptr) {
            conn->ws->close();
          }
        }
      });
    }
  }

  static void Close(ConnectionHdl hdl, int code, std::string_view msg = "") {
    if (auto conn = hdl.lock()) {
      std::string m(msg.data(), msg.length());
      conn->loop->defer([hdl, m = std::move(m), code](){
        if (auto conn = hdl.lock()) {
          if (conn->ws != nullptr) {
            conn->ws->end(code, m);
          }
        }
      });
    }
  }

 public:
  Server(): //cps_limiter_(FLAGS_ws_global_cps_limit_capacity, FLAGS_ws_global_cps_limit_rate_s),
            thread_cnt_(1),
            ws_receive_qps(&ws_receive),
            ws_connection_cps(&ws_connection),
            ws_connection_count(&Server::connection_count, this) {}
  ~Server() {}
  Server(Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  std::shared_ptr<HandleContext> MakeHandleContext(T&& data) {
    return std::make_shared<HandleContext>(HandleContext{
        .server = this,
        .loop = uWS::Loop::get(),
        .ws = nullptr,
        .data = std::move(data)
      });
  }

  void SetWebsockHandler(Handles&& handlers) {
    std::swap(hdls_, handlers);
  }

  template<class Fn>
  void AddHttpHandler(const std::string& pattern, Fn&& fn) {
    http_handlers_.emplace(pattern, std::move(fn));
  }

  bool Init(int port, int threadCnt) {
    ws_send.expose(absl::StrCat("websocket_", port, "_send"));
    ws_receive_qps.expose(absl::StrCat("websocket_", port, "_receive_qps"));
    ws_connection_cps.expose(absl::StrCat("websocket_", port, "_connection_cps"));
    ws_connection_count.expose(absl::StrCat("websocket_", port, "_connection_count"));

    CHECK(threadCnt <= kMaxThreadCnt);
    thread_cnt_ = threadCnt;
    for (int i = 0; i < threadCnt; ++i) {
      thread_vec_.push_back(
        new std::thread(std::bind(&Server::InitImpl_, this, i, port))
      );
    }
    const FinishFlagType finished = (FinishFlagType)-1 >> (kMaxThreadCnt - threadCnt);
    std::unique_lock lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(1000), [=](){ return finished == finished_flag_; });
    return finished == finished_flag_;
  }

  void Stop() {}

  void Join() {
    for (int i = 0; i < thread_cnt_; ++i) {
      thread_vec_[i]->join();
    }
  }

  static int64_t connection_count(void *p) {
    return ((Server*)p)->connection_count_.load(std::memory_order_relaxed);
  }

 private:

  void InitImpl_(int cnt, int port) {
    auto endpontPtr = std::make_shared<UWSServer>();
    //此模版类中调用其他 模版类的 模版函数
    endpontPtr->template ws<PerSocketData>("/*", {
      .compression = uWS::SHARED_COMPRESSOR,
      .maxPayloadLength = 16 * 1024,
      .idleTimeout = 6000000,  //空闲状态超时关闭
      .maxBackpressure = 1 * 1024 * 1024,
      /* Handlers */
      .upgrade = std::bind(&Server::Upgrade_, this,
                           std::placeholders::_1,
                           std::placeholders::_2,
                           std::placeholders::_3),
      .open = std::bind(&Server::Open_, this, std::placeholders::_1),
      .message = std::bind(&Server::Message_,
                           this,
                          std::placeholders::_1,
                          std::placeholders::_2,
                          std::placeholders::_3),
      .drain = nullptr,
      .ping = std::bind(&Server::Ping_, this, std::placeholders::_1),
      .pong = std::bind(&Server::Pong_, this, std::placeholders::_1),
      .close =std::bind(&Server::Close_,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3),
    });

    for (auto it: http_handlers_) {
      endpontPtr->get(it.first, [=](auto* resp, auto* req) {
        it.second(resp, req);
      });
      endpontPtr->post(it.first, [=](auto* resp, auto* req) {
        it.second(resp, req);
      });
    }
    bool ok = false;
    endpontPtr->listen(port, [&ok, port](auto *token){
      if (token) {
        ok = true;
        LOG(INFO) << "Thread " << std::this_thread::get_id() << " listening on port " << port << std::endl;
      } else {
        ok = false;
        LOG(ERROR) << "Thread " << std::this_thread::get_id() << " failed to listen on port " << port << std::endl;
      }
    });

    {
      std::lock_guard lk(mu_);
      if (ok) {
        finished_flag_ |= 1 << cnt;
      }
    }
    cv_.notify_one();
    if (ok) {
      endpontPtr->run();
    }
  }

  void Upgrade_(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req, struct us_socket_context_t* socket) {
    auto ali_ip = req->getHeader("ali-cdn-real-ip");
    auto ip = ali_ip.empty() ? req->getHeader("x-forwarded-for") : ali_ip;
    auto context = std::make_shared<HandleContext>(HandleContext{
        .server = this,
        .loop = uWS::Loop::get(),
        .ws = nullptr,
        .ip = std::string(ip.substr(0, ip.find_first_of(','))),
        .ua = std::string(req->getHeader("user-agent"))
      });

    hdls_.on_upgrade(res, req, context);

    res->template upgrade<PerSocketData>({
        .handle_context = context
      },
      req->getHeader("sec-websocket-key"),
      req->getHeader("sec-websocket-protocol"),
      req->getHeader("sec-websocket-extensions"),
      socket
      );
  }

  void Open_(uWS::WebSocket<SSL, true>* ws) {
    Context ctx{
      .cpuwide_time_us = butil::cpuwide_time_us()
    };

    // if (!cps_limiter_.Accquire(1)) {
    //   ws->end(1013, "Server is busy, try again later");
    //   LOG_EVERY_SECOND(ERROR) << "Websocket open falied, cps hits limit: capacity=" << (int)FLAGS_ws_global_cps_limit_capacity
    //              << ", rate=" << (int)FLAGS_ws_global_cps_limit_rate_s << "/s"
    //              << ", remote_addr=" << ws->getRemoteAddressAsText();
    //   return;
    // }

    if (connection_count_.fetch_add(1, std::memory_order_relaxed) > FLAGS_ws_connection_count_limit) {
      connection_count_.fetch_sub(1, std::memory_order_relaxed);
      ws->end(1013, "Server is busy, try again later");
      LOG_EVERY_SECOND(ERROR) << "Websocket open failed, connection count hits limit: "
                              << FLAGS_ws_connection_count_limit;
      return;
    }

    auto user_data = static_cast<PerSocketData*>(ws->getUserData());
    user_data->opened = true;
    user_data->handle_context->ws = ws;
    // user_data->limiter.Change(FLAGS_ws_connection_uplink_qps_limit_capacity,
    //                           FLAGS_ws_connection_uplink_qps_limit_rate_s);
    user_data->handle_context->loop = uWS::Loop::get();

    if (user_data->handle_context->ip.empty()) {
      user_data->handle_context->ip = ws->getRemoteAddressAsText();
    }

    ws_connection << 1;

    ctx.hdl = user_data->handle_context;
    hdls_.on_open(ctx);
  }

  void Message_(uWS::WebSocket<SSL, true>* ws, std::string_view message, uWS::OpCode opCode) {
    Context ctx{
      .cpuwide_time_us = butil::cpuwide_time_us()
    };

    auto user_data = static_cast<PerSocketData *>(ws->getUserData());
    // if (!user_data->limiter.Accquire(1)) {
    //   LOG(WARNING) << "Websocket message: received too many message, connection_id="
    //                << user_data->handle_context->data.connection_id;
    //   return;
    // }

    std::string msg(message.data(), message.size());
    ws_receive << 1;

    ctx.hdl = user_data->handle_context;
    hdls_.on_message(ctx, msg, opCode);
  }

  void Ping_(uWS::WebSocket<SSL, true>* ws) {
    auto user_data = static_cast<PerSocketData *>(ws->getUserData());

    Context ctx{
      .cpuwide_time_us = butil::cpuwide_time_us(),
      .hdl = user_data->handle_context
    };

    hdls_.on_ping(ctx);
  }

  void Pong_(uWS::WebSocket<SSL, true>* ws) {
    auto user_data = static_cast<PerSocketData *>(ws->getUserData());

    Context ctx{
      .cpuwide_time_us = butil::cpuwide_time_us(),
      .hdl = user_data->handle_context
    };

    hdls_.on_pong(ctx);
  }

  void Close_(uWS::WebSocket<SSL, true>* ws, int code, std::string_view message) {
    Context ctx{
      .cpuwide_time_us = butil::cpuwide_time_us()
    };

    auto user_data = static_cast<PerSocketData *>(ws->getUserData());
    if (!user_data->opened) {
      return;
    }

    connection_count_.fetch_sub(1, std::memory_order_relaxed);

    user_data->handle_context->ws = nullptr;
    ctx.hdl = user_data->handle_context;
    hdls_.on_close(ctx, code, message);
  }

  // base::RateLimiter cps_limiter_;
  std::atomic<int64_t> connection_count_{0};

  Handles hdls_;
  std::vector<std::thread *> thread_vec_;
  int thread_cnt_;
  std::mutex mu_;
  FinishFlagType finished_flag_{0};
  std::condition_variable cv_;

  std::map<std::string, std::function<void(uWS::HttpResponse<SSL>*, uWS::HttpRequest*)>> http_handlers_;

  bvar::LatencyRecorder ws_send;
  bvar::Adder<int64_t> ws_receive;
  bvar::PerSecond<bvar::Adder<int64_t>> ws_receive_qps;
  bvar::Adder<int64_t> ws_connection;
  bvar::PerSecond<bvar::Adder<int64_t>> ws_connection_cps;
  bvar::PassiveStatus<int64_t> ws_connection_count;
};

}//namespace base
