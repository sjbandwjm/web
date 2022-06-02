#include "manager.h"
#include <gflags/gflags.h>

DEFINE_int32(port, 1024, "chat server listen port");
namespace logging {
DECLARE_int32(minloglevel);
}


int main(int argc, char* argv[]) {
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  VLOG(1) << "start chat server port:" << FLAGS_port
            << " log_level:" << logging::FLAGS_minloglevel;

  jiayou::chat::ChatServer server;
  server.InitAndRun(FLAGS_port);
  server.Join();
}