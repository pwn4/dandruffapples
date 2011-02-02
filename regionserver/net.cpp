#include "net.h"

#include <iostream>
#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "../common/ports.h"
#include "../common/timestep.pb.h"

using namespace std;

namespace net {
  void run() {
    int timingsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(0 > timingsock) {
      perror("Failed to create timing socket");
      return;
    }

    struct sockaddr_in timingaddr;
    memset(&timingaddr, 0, sizeof(struct sockaddr_in));
    timingaddr.sin_family = AF_INET;
    timingaddr.sin_port = htons(TIMING_PORT);
    timingaddr.sin_addr.s_addr = INADDR_ANY;

    if(0 > bind(timingsock, (struct sockaddr *)&timingaddr, sizeof(struct sockaddr_in))) {
      perror("Failed to bind timing socket");
      close(timingsock);
      return;
    }

    if(0 > listen(timingsock, 0)) {
      perror("Failed to listen on timing socket");
      close(timingsock);
      return;
    }
    
    cout << "Waiting for timing server..." << flush;

    int timingfd = accept(timingsock, NULL, NULL);

    if(0 > timingfd) {
      perror("Failed to accept connection from timing server");
      close(timingsock);
      return;
    }
    cout << " got connection!" << endl;

    TimestepUpdate timestep;
    TimestepDone tsdone;
    bool done = false;
    while(!done) {
      done = done && timestep.ParseFromFileDescriptor(timingfd);
      cout << "Got timestep " << timestep.timestep() << endl;
      done = done && tsdone.SerializeToFileDescriptor(timingfd);
      cout << "Replied." << endl;
    }

    cout << "Connection to timing server lost." << endl;

    // Clean up
    shutdown(timingfd, SHUT_RDWR);
    close(timingsock);
  }
}
