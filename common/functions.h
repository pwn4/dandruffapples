#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

enum MessageType {
  TIMESTEPUPDATE,
  TIMESTEPDONE,
};

//struct for parsePacket to return
struct protoPacket {
  std::string packetData;
  int packetType;
};


protoPacket parsePacket(std::string * packetBuffer);

std::string makePacket(MessageType protoType, void * protoObject);

#endif
