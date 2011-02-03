#include "net.h"

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
      close(fd);
      return 0;
    }

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
  
  void run(const char *clockaddr) {
    int clockfd = do_connect(clockaddr, CLOCK_PORT);
    if(0 > clockfd) {
      perror("Failed to connect to clock server");
      return;
    }

    cout << " got connection!" << endl << "Waiting for timestep..." << flush;

    TimestepUpdate timestep;
    TimestepDone tsdone;
    tsdone.set_done(true);
    
    bool error = false;
    while(!error) {
      error = !timestep.ParseFromFileDescriptor(clockfd);
      cout << " Got timestep " << timestep.timestep() << endl;

      if(!error) {
        error = !tsdone.SerializeToFileDescriptor(clockfd);
        cout << "Done." << endl << "Waiting for timestep..." << flush;
      }
    }

    cout << " Connection to clock server lost." << endl;

    // Clean up
    shutdown(clockfd, SHUT_RDWR);
    close(clockfd);
  }
}
