#include "mssconnection.h"

#define MSS_RATIO 0.9

namespace net {
  MSSConnection::MSSConnection(int epoll, int flags_, int fd_) : EpollConnection(epoll, flags_, fd_) {
    mss = get_mss(fd_);
  }
  MSSConnection::MSSConnection(int epoll, int flags_, int fd_, Type type_) : EpollConnection(epoll, flags_, fd_, type_) {
    mss = get_mss(fd_);
  }

  void MSSConnection::push(MessageType typeTag, const google::protobuf::MessageLite &message) {
    queue.push(typeTag, message);
    if(!writing && queue.remaining() >= (MSS_RATIO*mss)) {
      set_writing(true);
    }
  }

  void MSSConnection::force_writing() {
    set_writing(true);
  }
}
