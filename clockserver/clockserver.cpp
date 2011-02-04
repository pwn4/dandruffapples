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
#include "../common/ports.h"

// AAA.BBB.CCC.DDD:EEEEE\n\0
#define ADDR_LEN (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 5 + 2)

using namespace std;

int main(int argc, char **argv) {
  unsigned server_count = argc > 1 ? atoi(argv[1]) : 1;
  int *servers = new int[server_count];

  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(0 > sock) {
    perror("Failed to create socket");
    return 1;
  }

  {
    int yes = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("Failed to reuse existing socket");
      return 1;
    }
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
  bool *doned = new bool[server_count];
  
  
  for(unsigned i = 0; i < 10; ++i) {
    cout << "Sending timestep " << i << endl;
    update.set_timestep(i);
    
    std::stringstream ss;
    std::string msg;
    //add the proto object data
    update.SerializeToString(&msg);
    //add the packet length and proto id to the front
    ss << "0" << '\0' << msg.length() << '\0' << msg;
    
    for(unsigned j = 0; j < server_count; ++j) {
        send(servers[j], ss.str().c_str(), ss.str().length(), 0);
    }
    
    //clear the array
    for(int j = 0; j < server_count; j++)
        doned[j] = false;
        
    //wait for done from each server
    
  }

  for(unsigned i = 0; i < server_count; ++i) {
    shutdown(servers[i], SHUT_RDWR);
    close(servers[i]);
  }
  delete[] servers;
  return 0;
}
