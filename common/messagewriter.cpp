#include "messagewriter.h"

#include <cstring>

#include "except.h"

MessageWriter::MessageWriter(int fd, size_t prealloc) : _fd(fd), _written(0), _msglen(0), _blocklen(0), _buflen(prealloc), _buffer(new uint8_t[_buflen]) {}

MessageWriter::MessageWriter(int fd, MessageType typeTag, const google::protobuf::MessageLite *message) : _fd(fd), _written(0), _msglen(0), _buflen(0), _buffer(NULL) {
  init(typeTag, message);
}

MessageWriter::MessageWriter(const MessageWriter &m) {
  _fd = m._fd;
  _written = m._written;
  _msglen = m._msglen;
  _blocklen = m._blocklen;
  _buflen = m._buflen;
  _buffer = new uint8_t[_buflen];
  memcpy(_buffer, m._buffer, _msglen);
}

MessageWriter::~MessageWriter()  {
  if(_buffer) {
    delete[] _buffer;
  }
}

void MessageWriter::init(MessageType typeTag, const google::protobuf::MessageLite *message) {
  _msglen = message->ByteSize();
  _blocklen = _msglen + sizeof(uint8_t) + sizeof(uint16_t);
  if(_buflen < _blocklen) {
    if(_buffer) {
      delete[] _buffer;
    }
    _buflen = _blocklen;
    _buffer = new uint8_t[_buflen];
  }
  _buffer[0] = typeTag;
  *(uint16_t*)(_buffer + sizeof(uint8_t)) = htons(_msglen);
  message->SerializeWithCachedSizesToArray(_buffer + sizeof(uint8_t) + sizeof(uint16_t));
}

bool MessageWriter::doWrite()  {
  ssize_t bytes;
  do {
    bytes = write(_fd, _buffer + _written, _blocklen - _written);
  } while(bytes < 0 && errno == EINTR);
  
  if(bytes < 0) {
    throw SystemError();
  }

  _written += bytes;

  if(_written == _blocklen) {
    _written = 0;
    return true;
  }
  
  return false;
}
