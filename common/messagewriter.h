#ifndef _MESSAGEWRITER_H_
#define _MESSAGEWRITER_H_

#include <sys/types.h>
#include <stdint.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>

#include "types.h"

template<class T>
class MessageWriter {
protected:
  int _fd;
  size_t _written, _len;
  uint8_t *_buffer;

public:
  MessageWriter(int fd, MessageType typeTag, const T *message) : _fd(fd), _written(0), _len(message->ByteSize()), _buffer(malloc(_len + sizeof(uint16_t))) {
    *(uint16_t*)_buffer = htons(typeTag);
    message->SerializeWithCachedSizesToArray(_buffer + sizeof(uint16_t));
  }
  ~MessageWriter();

  bool doWrite();
};

#endif
