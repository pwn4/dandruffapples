#include "net.h"

#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>

#include "ports.h"
#include "timestep.pb.h"
#include "except.h"

using namespace std;

namespace net {
  void set_blocking(int fd, bool state) {
    int flags;
    if(0 > (flags = fcntl(fd, F_GETFL, 0))) {
      throw SystemError("Failed to read existing flags on FD");
    }
    
    if(state) {
      flags &= ~O_NONBLOCK;
    } else {
      flags |= O_NONBLOCK;
    }
    
    if(0 > fcntl(fd, F_SETFL, flags)) {
      throw SystemError("Failed to set blocking state on FD");
    }
  }
  
    int do_listen(int port) {

        int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(0 > sock) {
          throw SystemError("Failed to create socket");
        }

        int yes = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
          throw SystemError("Failed to configure socket for reuse");
        }

        struct sockaddr_in clockaddr;
        memset(&clockaddr, 0, sizeof(struct sockaddr_in));
        clockaddr.sin_family = AF_INET;
        clockaddr.sin_port = htons(port);
        clockaddr.sin_addr.s_addr = INADDR_ANY;

        if(0 > bind(sock, (struct sockaddr *)&clockaddr, sizeof(struct sockaddr_in))) {
          throw SystemError("Failed to bind socket");
        }

        if(0 > listen(sock, 1)) {
          throw SystemError("Failed to listen on socket");
        }
        
        return sock;
    }

  int do_connect(const char *address, int port) {
    struct in_addr binary_addr;
    if(0 == inet_pton(AF_INET, address, &binary_addr)) {
      return 0;
    }
    return do_connect(binary_addr, port);
  }

  int do_connect(struct in_addr address, int port) {
    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(0 > fd) {
        return fd;
    }

    // Configure address and port
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(struct sockaddr_in));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr = address;
    
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

  int get_mss(int socket) {
    int value;
    socklen_t len = sizeof value;
    if(0 > getsockopt(socket, IPPROTO_TCP, TCP_MAXSEG, &value, &len)) {
      throw SystemError("Failed to determine maximum segment size on TCP socket");
    }
    return value;
  }
}
