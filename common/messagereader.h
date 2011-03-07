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
  size_t _bufsize, _headerpos, _msgpos;
  uint32_t _msglen;
  MessageType _type;
  // type + length
  const static size_t _headerlen = sizeof(uint8_t) + sizeof(uint32_t);
  
public:
  MessageReader(int fd, size_t initialSize = 256);
  MessageReader(const MessageReader&);
  ~MessageReader();

  bool doRead(MessageType *type, int *len, const void **buffer);
};

#endif
