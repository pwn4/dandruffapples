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
#include "../common/functions.h"

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
    std::string* packetBuffer = new std::string[server_count];
    int bufsize=1024;        /* a 1K socket input buffer. May need to increase later. */
    int recvsize;
    char *buffer = new char[bufsize];
    protoPacket nextPacket;

    //send timesteps continually
    int currentStep = 0;
    while(true){
        
        //create the timestep packet
        cout << "Sending timestep " << currentStep << endl;
        update.set_timestep(currentStep);
        
        string msg = makePacket(TIMESTEPUPDATE, &update);

        //broadcast to the servers
        for(unsigned j = 0; j < server_count; ++j) {
            send(servers[j], msg.c_str(), msg.length(), 0);
        }

        //wait for 'done' from each server. When rewriting this, should make it fair and check for messages from each server while waiting, instead of 'blocking' on servers until they're done.
        bool serverDoned;
        for(size_t j = 0; j < server_count; j++)
        {
            serverDoned = false;
            recvsize = recv(servers[j], buffer, bufsize, 0);
            while(recvsize > 0 && !serverDoned)
            {      
                //add recvsize characters to the packetBuffer. We have to parse them ourselves there.
                for(int k = 0; k < recvsize; k++)
                    packetBuffer[j].push_back(buffer[k]);

                //we may have multiple packets all in the buffer together. Tokenize them. Damn TCP streaming
                nextPacket = parsePacket(&packetBuffer[j]);
                while(nextPacket.packetType != -1)
                {
                    //parse that data with that sexy parse function
                    if(nextPacket.packetType == TIMESTEPDONE) //timestep done packet
                    {
                        //don't even both parseing it. If it's a done type, it's done.
                        serverDoned = true;
                        break;
                    }
                    
                    //don't do anything if its of a different type. Will add more later for different proto types

                    nextPacket = parsePacket(&packetBuffer[j]);
                }
            }
        }
        
        //delay to bind simulation speed for now
        sleep(2);
        currentStep++;

    }

    for(unsigned i = 0; i < server_count; ++i) {
    shutdown(servers[i], SHUT_RDWR);
    close(servers[i]);
    }
    delete[] servers;
    return 0;
}
