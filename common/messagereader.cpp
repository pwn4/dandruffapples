#include "messagereader.h"

#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>

#include "except.h"

MessageReader::MessageReader(int fd, size_t initialSize) : _fd(fd), _bufsize(initialSize > _headerlen ? initialSize : _headerlen), _headerpos(0), _msgpos(0) {
  _buffer = (uint8_t*)malloc(_bufsize);
}

MessageReader::MessageReader(const MessageReader& m) {
  _fd = m._fd;
  _bufsize = m._bufsize;
  _headerpos = m._headerpos;
  _msgpos = m._msgpos;
  _buffer = (uint8_t*)malloc(_bufsize);
  memcpy(_buffer, m._buffer, _msgpos + _headerpos);
}

MessageReader::~MessageReader() {
  free(_buffer);
}

bool MessageReader::doRead(MessageType *type, int *len, const void **buffer) {
  ssize_t bytes;
  // Read type, if necessary
  if(_headerpos < _headerlen) {
    // We're reading a new message; get just its type
    do {
      bytes = read(_fd, _buffer + _headerpos, _headerlen - _headerpos);
    } while(bytes < 0 && errno == EINTR);
    if(bytes < 0) {
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
        return false;
      }
      throw SystemError("Failed to read message type");
    } else if(bytes == 0) {
      throw EOFError();
    }
    
    _headerpos += bytes;

    if(_headerpos == _headerlen) {
      // Reorder bytes
      _msglen = ntohl(*((uint32_t*)(_buffer + 1)));
      // Realloc buffer as necessary
      while(_bufsize < (_msglen + _headerlen)) {
        _bufsize *= 2;
        _buffer = (uint8_t*)realloc(_buffer, _bufsize);
      }
      // No byte reordering necessary because it's one byte.
      _type = (MessageType)_buffer[0];
    } else {
      return false;
    }
  }

  if(_type == 0 || _type >= MSG_MAX) {
    throw UnknownMessageError();
  }

  if(_msglen == 0) {
    throw ZeroLengthMessageError();
  }

  // Read message body
  do {
    bytes = read(_fd, _buffer + _headerlen + _msgpos, _msglen - _msgpos);
  } while(bytes < 0 && errno == EINTR);
  if(bytes < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    throw SystemError("Failed to read message body");
  } else if(bytes == 0) {
    throw EOFError();
  }

  // Keep track of how much we've read
  _msgpos += bytes;
  
  if(_msgpos == _msglen) {
    // We've got the complete message.
    // Reset internal state to enable reuse
    _headerpos = 0;
    _msgpos = 0;

    // Make our data available
    if(type) {
      *type = _type;
    }
    *len = _msglen;
    *buffer = _buffer + _headerlen;

    return true;
  }
  
  return false;
}
