#include "messagewriter.h"

#include "except.h"

MessageWriter::MessageWriter(int fd, MessageType typeTag, const google::protobuf::MessageLite *message) :
  _fd(fd), _written(0), _len(message->ByteSize()),
  _buffer(new uint8_t[_len + sizeof(uint16_t)]) {
  *(uint16_t*)_buffer = htons(typeTag);
  message->SerializeWithCachedSizesToArray(_buffer + sizeof(uint16_t));
}

MessageWriter::~MessageWriter()  {
  delete[] _buffer;
}

bool MessageWriter::doWrite()  {
  ssize_t bytes;
  do {
    bytes = write(_fd, _buffer, _len);
  } while(bytes < 0 && errno == EINTR);
  
  if(bytes < 0) {
    throw SystemError();
  }

  _written += bytes;
  if(_written == _len) {
    return true;
  }
  
  return false;
}
