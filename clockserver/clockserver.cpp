#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sstream>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common/timestep.pb.h"

#include "../common/except.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagereader.h"
#include "../common/messagewriter.h"

// AAA.BBB.CCC.DDD:EEEEE\n\0
using namespace std;

int main(int argc, char **argv) {
  unsigned server_count = argc > 1 ? atoi(argv[1]) : 1;
  int *servers = new int[server_count];

  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(0 > sock) {
    perror("Failed to create socket");
    return 1;
  }

  int yes = 1;
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("Failed to reuse existing socket");
    return 1;
  }

  struct sockaddr_in clockaddr;
  memset(&clockaddr, 0, sizeof(struct sockaddr_in));
  clockaddr.sin_family = AF_INET;
  clockaddr.sin_port = htons(CLOCK_PORT);
  clockaddr.sin_addr.s_addr = INADDR_ANY;

  if(0 > bind(sock, (struct sockaddr *)&clockaddr, sizeof(struct sockaddr_in))) {
    perror("Failed to bind socket");
    close(sock);
    return 1;
  }

  if(0 > listen(sock, 1)) {
    perror("Failed to listen on socket");
    close(sock);
    return 1;
  }

  cout << "Waiting for region server connections" << flush;

  for(unsigned i = 0; i < server_count;) {
    do {
      servers[i] = accept(sock, NULL, NULL);
    } while(servers[i] < 0 && errno == EINTR);

    if(0 > servers[i]) {
      perror("Failed to accept connection");
      continue;
    }

    cout << "." << flush;
    ++i;
  }

  cout << " All region servers connected!" << endl;

  // Do stepping
  TimestepUpdate update;
  TimestepDone done;

  //send timesteps continually
  int currentStep = 0;
  bool error = false;
  while(!error) {
    //create the timestep packet
    update.set_timestep(currentStep);
        
    //broadcast to the servers
    cout << "Sending timestep " << currentStep << flush;
    for(size_t i = 0; i < server_count; ++i) {
      MessageWriter writer(servers[i], TIMESTEPUPDATE, &update);
      for(bool complete = false; !complete;) {
        complete = writer.doWrite();
      }

      cout << "." << flush;
    }
    cout << " Done." << endl;

    //wait for 'done' from each server. When rewriting this, should make it fair and check for messages from each server while waiting, instead of 'blocking' on servers until they're done.
    cout << "Waiting for responses" << flush;
    for(size_t i = 0; i < server_count; ++i) {
      MessageReader reader(servers[i], 16);
      MessageType type;
      size_t len;
      const void *buffer;
      for(bool complete = false; !complete;) {
        try {
          complete = reader.doRead(&type, &len, &buffer);
        } catch(EOFError e) {
          cout << " region server disconnected, shutting down" << flush;
          error = true;
          break;
        } catch(SystemError e) {
          cerr << " error performing network I/O: " << e.what() << flush;
          error = true;
          break;
        }
      }
      cout << "." << flush;
    }
    cout << " Done." << endl;
        
    //delay to bind simulation speed for now
    sleep(2);
    currentStep++;
  }

  for(size_t i = 0; i < server_count; ++i) {
    shutdown(servers[i], SHUT_RDWR);
    close(servers[i]);
  }
  delete[] servers;
  return 0;
}
