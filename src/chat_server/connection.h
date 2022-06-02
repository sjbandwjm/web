#pragma once
#include "frontend.h"

namespace jiayou::chat {

struct ConnectionInfo {
  std::string connection_id;
  std::string user_name;
  std::string user_id;
  bool is_login{false};
};

typedef websocket::Server<ConnectionInfo, false> Server;
typedef Server::Context ConnectionCtx;
}