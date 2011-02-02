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
    int clocksock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(0 > clocksock) {
      perror("Failed to create clock socket");
      return;
    }

    struct sockaddr_in clockaddr;
    memset(&clockaddr, 0, sizeof(struct sockaddr_in));
    clockaddr.sin_family = AF_INET;
    clockaddr.sin_port = htons(CLOCK_PORT);
    clockaddr.sin_addr.s_addr = INADDR_ANY;

    if(0 > bind(clocksock, (struct sockaddr *)&clockaddr, sizeof(struct sockaddr_in))) {
      perror("Failed to bind clock socket");
      close(clocksock);
      return;
    }

    if(0 > listen(clocksock, 0)) {
      perror("Failed to listen on clock socket");
      close(clocksock);
      return;
    }
    
    cout << "Waiting for clock server..." << flush;

    int clockfd = accept(clocksock, NULL, NULL);

    if(0 > clockfd) {
      perror("Failed to accept connection from clock server");
      close(clocksock);
      return;
    }
    cout << " got connection!" << endl;

    TimestepUpdate timestep;
    TimestepDone tsdone;
    bool done = false;
    while(!done) {
      done = done && timestep.ParseFromFileDescriptor(clockfd);
      cout << "Got timestep " << timestep.timestep() << endl;
      done = done && tsdone.SerializeToFileDescriptor(clockfd);
      cout << "Replied." << endl;
    }

    cout << "Connection to clock server lost." << endl;

    // Clean up
    shutdown(clockfd, SHUT_RDWR);
    close(clocksock);
  }
}
