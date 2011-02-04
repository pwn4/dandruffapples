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

    void run(const char *clockaddr) {
        int clockfd = do_connect(clockaddr, CLOCK_PORT);
        if(0 > clockfd) {
          printf("Failed to connect to clock server\n");
          exit(1);
        }

        cout << " Got connection!" << endl << "Waiting for timestep..." << flush;

        TimestepUpdate timestep;
        TimestepDone tsdone;
        tsdone.set_done(true);

        int bufsize=1024;        /* a 1K socket input buffer. May need to increase later. */
        int recvsize;
        char *buffer = new char[bufsize];
        std::string packetBuffer="";
        protoPacket nextPacket;

        recvsize = recv(clockfd,buffer,bufsize,0);
        while(recvsize > 0)
        {        
            //add recvsize characters to the packetBuffer. We have to parse them ourselves there.
            for(int i = 0; i < recvsize; i++)
                packetBuffer.push_back(buffer[i]);

            //we may have multiple packets all in the buffer together. Tokenize them. Damn TCP streaming
            nextPacket = parsePacket(&packetBuffer);
            while(nextPacket.packetType != -1)
            {
                //parse that data with that sexy parse function
                if(nextPacket.packetType == TIMESTEPUPDATE) //timestep done packet
                {
                    timestep.ParseFromString(nextPacket.packetData);
                    cout << "Timestep: " << timestep.timestep() << endl << flush;
                    
                    cout << "Sending Done..." << endl << flush;
                    
                    string msg = makePacket(TIMESTEPDONE, &tsdone);
                    
                    send(clockfd, msg.c_str(), msg.length(), 0);
                }
                
                //don't do anything if its of a different type. Will add more later for different proto types
                

                nextPacket = parsePacket(&packetBuffer);
            }

	        //get more data
	        recvsize = recv(clockfd,buffer,bufsize,0);
        }

        cout << " Connection to clock server lost." << endl;

        // Clean up
        shutdown(clockfd, SHUT_RDWR);
        close(clockfd);
    }
}
