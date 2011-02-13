#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>

namespace net {
  void set_blocking(int fd, bool state);
  int do_connect(const char *address, int port);
  int do_connect(struct in_addr address, int port);
  int do_listen(int port);
  int get_mss(int socket);
}

#endif
