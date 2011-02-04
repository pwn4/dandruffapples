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

using namespace std;

//struct for parsePacket to return
struct protoPacket {
    string packetData;
    int packetType;
} ;


protoPacket parsePacket(std::string * packetBuffer);

