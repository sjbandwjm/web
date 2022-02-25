#include "manager.h"
#include <gflags/gflags.h>

int main(int argc, char* argv[]) {
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  jiayou::chat::ChatServer server;
  server.InitAndRun(8000);
  server.Join();
}