#include "messagewriter.h"

#include "except.h"

MessageWriter::MessageWriter(int fd, MessageType typeTag, const google::protobuf::MessageLite *message) {
  _fd = fd;
  _written = 0;
  _msglen = message->ByteSize();
  _buflen = _msglen + sizeof(uint8_t) + sizeof(uint16_t);
  _buffer = new uint8_t[_buflen]; 
  _buffer[0] = typeTag;
  *(uint16_t*)(_buffer + sizeof(uint8_t)) = htons(_msglen);
  message->SerializeWithCachedSizesToArray(_buffer + sizeof(uint8_t) + sizeof(uint16_t));
}

MessageWriter::~MessageWriter()  {
  delete[] _buffer;
}

bool MessageWriter::doWrite()  {
  ssize_t bytes;
  do {
    bytes = write(_fd, _buffer + _written, _buflen - _written);
  } while(bytes < 0 && errno == EINTR);
  
  if(bytes < 0) {
    throw SystemError();
  }

  _written += bytes;
  if(_written == _buflen) {
    _written = 0;
    return true;
  }
  
  return false;
}
