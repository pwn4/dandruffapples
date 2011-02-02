#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// AAA.BBB.CCC.DDD:EEEEE\n\0
#define ADDR_LEN (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 5 + 2)

int main(int argc, char **argv) {
  size_t input_size = ADDR_LEN;
  char *input = (char*)malloc(ADDR_LEN);

  while(true) {
    printf("Enter a region server address: ");
    if(0 > getline(&input, &input_size, stdin)) {
      printf("\nGot EOF.  Shutting down.\n");
      return 0;
    }
    size_t input_len = strlen(input);

    char *port;
    for(port = input; *port != ':' && (port - input) < input_len; ++port);
    if((port - input) == input_len) {
      fprintf(stderr, "A port must be specified!\n");
    } else {
      // Split the string
      *port = '\0';
      ++port;
      // Strip newline
      char *end;
      for(end = port; *end != '\n'; ++end);
      if(end == port) {
        fprintf(stderr, "Invalid port.\n");
      } else {
        *end = '\0';
      
        printf("Got address %s, port %s\n", input, port);
      }
    }
  } 

  free(input);
  return 0;
}
