#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>
#include <sys/epoll.h>

#include "messagequeue.h"
#include "messagereader.h"

namespace net {
  // Not actually socket-exclusive
  void set_blocking(int fd, bool state);
  bool get_blocking(int fd);
  
  int do_connect(const char *address, int port, unsigned timeout = 0);
  int do_connect(struct in_addr address, int port, unsigned timeout = 0);
  int do_listen(int port);
  int get_mss(int socket);

  struct connection {
    enum Type {
      UNSPECIFIED,
      REGION_LISTEN,
      CONTROLLER_LISTEN,
      WORLDVIEWER_LISTEN,
      CLIENT_LISTEN,
      CLOCK,
      CONTROLLER,
      REGION,
      CLIENT,
      WORLDVIEWER,
      STDIN
    } type;

    int fd;
    MessageReader reader;
    MessageQueue queue;

    connection(int fd_) : type(UNSPECIFIED), fd(fd_), reader(fd_), queue(fd_) {}
    connection(int fd_, Type type_) : type(type_), fd(fd_), reader(fd_), queue(fd_) {}
  };

  class EpollConnection : public connection {
  protected:
    bool reading, writing;
    int epoll;
    struct epoll_event event;

  public:
    EpollConnection(int epoll, int flags_, int fd_);
    EpollConnection(int epoll, int flags_, int fd_, Type type_);

    void set_reading(bool state);
    void set_writing(bool state);
  };
}

#endif
