#include "net.h"

#include <cstdio>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#include "../common/timestep.pb.h"

namespace net {
  void run() {
    int timingsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(timingsock < 0) {
      perror("Failed to create timing socket");
      return;
    }

    close(timingsock);
  }
}
