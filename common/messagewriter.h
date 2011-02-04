#ifndef _MESSAGEWRITER_H_
#define _MESSAGEWRITER_H_

#include <sys/types.h>
#include <stdint.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>

#include <google/protobuf/message_lite.h>

#include "types.h"

class MessageWriter {
protected:
  int _fd;
  size_t _written, _msglen, _buflen;
  uint8_t *_buffer;

public:
  MessageWriter(int fd, MessageType typeTag, const google::protobuf::MessageLite *message);
  MessageWriter(int fd, size_t prealloc = 32);
  ~MessageWriter();

  void init(MessageType typeTag, const google::protobuf::MessageLite *message);

  bool doWrite();
};

#endif
