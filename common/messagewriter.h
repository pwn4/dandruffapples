#ifndef _MESSAGEWRITER_H_
#define _MESSAGEWRITER_H_

#include <sys/types.h>
#include <stdint.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>

#include "except.h"

template<class T>
class MessageWriter {
protected:
  int _fd;
  size_t _written, _len;
  uint8_t *buffer;

public:
  MessageWriter(int fd, const T *message) : _fd(fd), _written(0), _len(message->ByteSize()), buffer(malloc(_len)) {
    message->SerializeWithCachedSizesToArray(buffer);
  }

  bool doWrite();
};

#endif
