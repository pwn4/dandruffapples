#include "messagequeue.h"

#include <cstring>

#include "except.h"

MessageQueue::MessageQueue(int fd, size_t prealloc) : _fd(fd), _bufsize(prealloc), _appendpt(0), _writept(0), _buffer(new uint8_t[_bufsize]) {}

MessageQueue::MessageQueue(const MessageQueue &m) {
  _fd = m._fd;
  _bufsize = m._bufsize;
  _appendpt = m._appendpt;
  _writept = m._writept;
  _buffer = new uint8_t[_bufsize];
  memcpy(_buffer + _writept, m._buffer + _writept, remaining());
}

MessageQueue::~MessageQueue()  {
  if(_buffer) {
    delete[] _buffer;
  }
}

void MessageQueue::push(MessageType typeTag, const google::protobuf::MessageLite &message) {
  if(!message.IsInitialized()) {
    throw UninitializedMessageError();
  }

  // Work out sizes
  uint32_t msglen = message.ByteSize();
  if(msglen < 1) {
    throw UninitializedMessageError();
  }
  int blocklen = msglen + sizeof(uint8_t) + sizeof(uint32_t);

  // Ensure we have a place to put it
  if(_bufsize < _appendpt + blocklen) {
    int datalen = remaining();
    if(_writept > 0 &&
       blocklen < _writept + (_bufsize - _appendpt)) {
      // We have enough discontinuous space; rearrange it.  We could
      // do this less often with some circularness, but it could be a
      // bit hairy.
      memmove(_buffer, _buffer + _writept, datalen);
      _appendpt -= _writept;
      _writept = 0;
    } else {
      // We need a bigger buffer
      int newsize = _bufsize ? _bufsize * 2 : 256;
      while(newsize < datalen + blocklen) {
        newsize *= 2;
      }
      uint8_t *newbuf = new uint8_t[newsize];
      if(_buffer) {
        memcpy(newbuf, _buffer + _writept, datalen);
        delete[] _buffer;
      }
      _buffer = newbuf;
      _appendpt -= _writept;
      _writept = 0;
    }
  }
  
  //if we try to WRITE a zero length message, throw an error too
  if(msglen == 0) {
    std::runtime_error("Tried to write a message length of 0!");
  }

  // Enter message into buffer
  _buffer[_appendpt] = typeTag;
  *(uint32_t*)(_buffer + _appendpt + sizeof(uint8_t)) = htonl(msglen);
  message.SerializeWithCachedSizesToArray(_buffer + _appendpt + sizeof(uint8_t) + sizeof(uint32_t));

  _appendpt += blocklen;
}

bool MessageQueue::doWrite() {
  ssize_t bytes;
  do {
    bytes = write(_fd, _buffer + _writept, remaining());
  } while(bytes < 0 && errno == EINTR);
  
  if(bytes < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      // We're only supposed to be called when the socket is writable,
      // but no harm in being resilient.
      return false;
    }
    throw SystemError("Failed to write message");
  }

  _writept += bytes;

  if(_writept == _appendpt) {
    _writept = 0;
    _appendpt = 0;
    return true;
  }
  
  return false;
}
