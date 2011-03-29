#ifndef _MSSCONNECTION_H_
#define _MSSCONNECTION_H_

#include <google/protobuf/message_lite.h>

#include "net.h"

namespace net {
  class MSSConnection : public EpollConnection {
  protected:
    int mss;

  public:
    MSSConnection(int epoll, int flags_, int fd_);
    MSSConnection(int epoll, int flags_, int fd_, Type type_);

    void push(MessageType typeTag, const google::protobuf::MessageLite &message);
    void force_writing();
  };
}

#endif
