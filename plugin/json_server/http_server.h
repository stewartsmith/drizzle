#include <event.h>
#include <evhttp.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
using namespace std;

namespace drizzle_plugin {
namespace json_server{

class HTTPServer {
public:
  HTTPServer() {}
  ~HTTPServer() {}
protected:
  int BindSocket(const char *address, int port);
};
}
}
