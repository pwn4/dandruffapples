#ifndef _NET_H_
#define _NET_H_

namespace net {
  int do_connect(const char *address, int port);
  int do_listen(int port);
}

#endif
