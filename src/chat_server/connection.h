#pragma once

namespace jiayou::chat {

struct ConnectionInfo {
  std::string connection_id;
};

typedef websocket::Server<ConnectionInfo, false> Server;
typedef Server::Context ConnectionHdl;
}