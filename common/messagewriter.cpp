#include "messagewriter.h"

template<class T>
bool MessageWriter<T>::doWrite()  {
  ssize_t bytes;
  do {
    bytes = write(_fd, buffer, _len);
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
