#ifndef _MESSAGEQUEUE_H_
#define _MESSAGEQUEUE_H_

#include <queue>
#include <utility>
#include <tr1/memory>

#include <google/protobuf/message_lite.h>

#include "messagewriter.h"

typedef std::tr1::shared_ptr<const google::protobuf::MessageLite> msg_ptr;

class MessageQueue {
protected:
  MessageWriter _writer;
  std::queue<std::pair<MessageType, msg_ptr> > _waiting;

public:
  MessageQueue(int fd, size_t prealloc = 32);

  void push(MessageType type, msg_ptr message);
  bool doWrite();

  bool empty() const;
};

#endif
