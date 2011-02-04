#include "messagereader.h"

#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>

#include "except.h"

MessageReader::MessageReader(int fd, size_t initialSize) : _fd(fd), _bufsize(initialSize), _typepos(0), _lenpos(0), _bufpos(0) {
  _buffer = (uint8_t*)malloc(_bufsize);
}

MessageReader::~MessageReader() {
  free(_buffer);
}

bool MessageReader::doRead(MessageType *type, const void **buffer) {
  ssize_t bytes;
  // Read type, if necessary
  if(_typepos < sizeof(_type)) {
    // We're reading a new message; get just its type
    do {
      bytes = read(_fd, &_type + _typepos, sizeof(_type) - _typepos);
    } while(bytes < 0 && errno == EINTR);
    if(bytes <= 0) {
      throw SystemError();
    }
    
    _typepos += bytes;

    // No byte reordering necessary because it's one byte.
    
    return false;
  }

  // Read length, if necessary
  if(_lenpos < sizeof(_msglen)) {
    // We're (still) reading a new message; get just its length
    do {
      bytes = read(_fd, &_msglen + _lenpos, sizeof(_msglen) - _lenpos);
    } while(bytes < 0 && errno == EINTR);
    if(bytes <= 0) {
      throw SystemError();
    }
    
    _lenpos += bytes;

    if(_lenpos == sizeof(_msglen)) {
      // We just finished reading message length
      // Handle byte ordering
      _msglen = ntohs(_msglen);
      // Be sure we have enough space for the entire message
      if(_bufsize < _msglen) {
        _bufsize *= 2;
        _buffer = (uint8_t*)realloc(_buffer, _bufsize);
      }
    }

    return false;
  }

  // Read message body
  do {
    bytes = read(_fd, _buffer + _bufpos, _msglen - _bufpos);
  } while(bytes < 0 && errno == EINTR);
  if(bytes <= 0) {
    throw SystemError();
  }

  // Keep track of how much we've read
  _bufpos += bytes;
  
  if(_bufpos == _msglen) {
    // We've got the complete message.
    // Reset internal state to enable reuse
    _typepos = 0;
    _lenpos = 0;
    _bufpos = 0;
    // Make our data available
    *buffer = _buffer;
    *type = (MessageType)_type;
    return true;
  }
  
  return false;
}
