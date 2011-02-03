#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common/timestep.pb.h"

// AAA.BBB.CCC.DDD:EEEEE\n\0
#define ADDR_LEN (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 5 + 2)

using namespace std;

char *parse_port(char *input) {
  size_t input_len = strlen(input);
  char *port;
  
  for(port = input; *port != ':' && (port - input) < input_len; ++port);
  
  if((port - input) == input_len) {
    return NULL;
  } else {
    // Split the string
    *port = '\0';
    ++port;
    // Strip newline
    char *end;
    for(end = port; *end != '\n'; ++end);
    if(end == port) {
      return NULL;
    } else {
      *end = '\0';
      return port;
    }
  }
}

int do_connect(char *address, char *port) {
  int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(0 > fd) {
    return fd;
  }

  // Set non-blocking
  int flags;
  if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  if(0 > fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
    int tmp = errno;
    close(fd);
    errno = tmp;
    return -1;
  }

  // Configure address and port
  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(struct sockaddr_in));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(atoi(port));
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

int main(int argc, char **argv) {
  size_t input_size = ADDR_LEN;
  char *input = (char*)malloc(ADDR_LEN);

  TimestepUpdate timestep;
  timestep.set_timestep(0);

  while(true) {
    cout << "Enter a region server address: " << flush;
    if(0 > getline(&input, &input_size, stdin)) {
      cout << endl << "Got EOF, shutting down." << endl;
      return 0;
    }

    char *port = parse_port(input);
    if(!port) {
      cerr <<  "Address must be in the format of IP:port" << endl;
      continue;
    }
    cout << "Got address " << input << ", port " << port << endl;

    int fd = do_connect(input, port);
    if(fd < 0) {
      perror("Failed to connect");
      continue;
    } else if(fd == 0) {
      cerr << "Invalid address and/or port." << endl;
      continue;
    }
    timestep.SerializeToFileDescriptor(fd);
    cout << "Sent timestep " << timestep.timestep() << endl;
    timestep.set_timestep(timestep.timestep() + 1);
    close(fd);
  } 

  free(input);
  return 0;
}
