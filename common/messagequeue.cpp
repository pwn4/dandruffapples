#include "messagequeue.h"

using namespace std;
using namespace tr1;

MessageQueue::MessageQueue(int fd, size_t prealloc) : _writer(fd, prealloc) {}

void MessageQueue::push(MessageType type, msg_ptr message) {
  _waiting.push(pair<MessageType, msg_ptr>(type, message));
}

bool MessageQueue::doWrite() {
  if(!_writer.writing()) {
    if(_waiting.empty()) {
      return true;
    }
    _writer.init(_waiting.front().first, *_waiting.front().second);
  }
  if(_writer.doWrite()) {
    if(_waiting.empty()) {
      return true;
    }
    _waiting.pop();
  }

  return false;
}

bool MessageQueue::empty() const {
  return _waiting.empty() && !_writer.writing();
}
