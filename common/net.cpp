#include "net.h"

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

#include "../common/ports.h"
#include "../common/timestep.pb.h"
#include "../common/functions.h"

using namespace std;

namespace net {
  int do_connect(const char *address, int port) {
    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(0 > fd) {
     return fd;
    }

    // Set non-blocking
    // int flags;
    // if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
    //   flags = 0;
    // if(0 > fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
    //   int tmp = errno;
    //   close(fd);
    //   errno = tmp;
    //   return -1;
    // }

    // Configure address and port
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(struct sockaddr_in));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    if(0 == inet_pton(AF_INET, address, &sockaddr.sin_addr)) {
      printf("Error: network address invalid... %s\n", address);
      close(fd);
      return -1;
    }
    
    printf("Connecting...\n");
    // Initiate connection
    if(0 > connect(fd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in))) {     
      if(errno != EINPROGRESS) {
        int tmp = errno;
        close(fd);
        errno = tmp;
        return -1;
      }
    }

    return fd;
  }


}
