#ifndef _MESSAGEWRITER_H_
#define _MESSAGEWRITER_H_

#include <stdexcept>

#include <sys/types.h>
#include <stdint.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>

#include <google/protobuf/message_lite.h>

#include "types.h"

class UninitializedMessageError : public std::runtime_error {
public:
  UninitializedMessageError() : runtime_error("Attempted to write an uninitialized message!") {}
};

class MessageWriter {
protected:
  int _fd;
  size_t _written, _msglen, _blocklen, _buflen;
  uint8_t *_buffer;

public:
  MessageWriter(int fd, size_t prealloc = 32);
  MessageWriter(const MessageWriter&);
  ~MessageWriter();

  void init(MessageType typeTag, const google::protobuf::MessageLite *message);

  bool doWrite();
  bool writing();
};

#endif
