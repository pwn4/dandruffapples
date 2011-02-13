#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>

#include "messagequeue.h"
#include "messagereader.h"

namespace net {
  void set_blocking(int fd, bool state);
  int do_connect(const char *address, int port);
  int do_connect(struct in_addr address, int port);
  int do_listen(int port);
  int get_mss(int socket);

  struct connection {
    enum Type {
      UNSPECIFIED,
      REGION_LISTEN,
      CONTROLLER_LISTEN,
      PNGVIEWER_LISTEN,
      CLOCK,
      CONTROLLER,
      REGION,
      CLIENT,
      PNGVIEWER
    } type;

    int fd;
    MessageReader reader;
    MessageQueue queue;

    connection(int fd_) :
      type(UNSPECIFIED), fd(fd_), reader(fd_), queue(fd_) {
    }
    connection(int fd_, Type type_) :
      type(type_), fd(fd_), reader(fd_), queue(fd_) {
    }
  };
}

#endif
