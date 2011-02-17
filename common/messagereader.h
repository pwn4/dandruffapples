#ifndef _MESSAGEREADER_H_
#define _MESSAGEREADER_H_

#include <stdexcept>

#include <sys/types.h>
#include <stdint.h>

#include "types.h"

class ZeroLengthMessageError : public std::runtime_error {
public:
  ZeroLengthMessageError() : runtime_error("Read a message length of 0!") {}
};

class UnknownMessageError : public std::runtime_error {
public:
  UnknownMessageError() : runtime_error("Read an unknown message type!") {}
};

class MessageReader {
protected:
  int _fd;
  uint8_t *_buffer;
  size_t _bufsize, _typepos, _lenpos, _bufpos;
  uint32_t _msglen;
  uint8_t _type;
  
public:
  MessageReader(int fd, size_t initialSize = 256);
  MessageReader(const MessageReader&);
  ~MessageReader();

  bool doRead(MessageType *type, size_t *len, const void **buffer);
};

#endif
