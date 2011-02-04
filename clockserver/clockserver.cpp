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
#include "../common/net.h"

// AAA.BBB.CCC.DDD:EEEEE\n\0
#define ADDR_LEN (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 5 + 2)
using namespace std;


//define variables
char configFileName [30] = "config";

int max_controllers = 10;
unsigned server_count = 1;
int *servers;
int *controllers;
int freeController = 0;
int sock, controllerSock;



//this function parses any minimal command line arguments and uses their values
void parseArguments(int argc, char* argv[])
{
	//loop through the arguments
	for(int i = 0; i < argc; i++)
	{
		//if it's a configuration file name...
		if(strcmp(argv[i], "-c") == 0)
		{
			strcpy(configFileName, argv[i+1]);
		
			printf("Using config file: %s\n", configFileName);
			
			i++; //increment the loop counter for one argument
		}
	}
}


//this function loads the config file so that the server parameters don't need to be added every time
void loadConfigFile()
{
	//open the config file
	FILE * fileHandle;
	fileHandle = fopen (configFileName,"r");
	
	//create a read buffer. No line should be longer than 200 chars long.
	char readBuffer [200];
	char * token;
	
	if (fileHandle != NULL)
	{
		while(fgets (readBuffer , sizeof(readBuffer) , fileHandle) != 0)
		{	
			token = strtok(readBuffer, " \n");
			
			//if it's a REGION WIDTH definition...
			if(strcmp(token, "NUMSERVERS") == 0){
				token = strtok(NULL, " \n");
				server_count = atoi(token);
				printf("NUMSERVERS: %d\n", server_count);
			}
			
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}


void initialize()
{
    //////////////SOCKET INIT///////////////////////////
    servers = new int[server_count];
    controllers = new int[max_controllers];
    
    sock = net::do_listen(CLOCK_PORT, true);
    controllerSock = net::do_listen(CONTROLLERS_PORT, false);
    
    if(sock < 0)
    {
        printf("Clock socket error: Server aborting...");
        exit(1);
    }
    
    if(controllerSock < 0)
    {
        printf("Controller socket error: Server aborting...");
        exit(1);
    }
}

void acceptRegionServers()
{
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
}

void run()
{
    // Do stepping
    TimestepUpdate update;
    TimestepDone done;
    std::string* packetBuffer = new std::string[server_count];
    int bufsize=1024;        /* a 1K socket input buffer. May need to increase later. */
    int recvsize;
    char *buffer = new char[bufsize];
    protoPacket nextPacket;

    //send timesteps continually - this is the main loop for the clock server
    int currentStep = 0;
    while(true){
    
        //check for new controller connections, given that there's space
        if(freeController < max_controllers)
        {
            controllers[freeController] = accept(controllerSock, NULL, NULL);
            if(controllers[freeController] < 0 && errno != EINTR)
                printf("Attempted controller connect: error in accepting.\n");
            else if(controllers[freeController] >= 0)
            {
                freeController++;
            }
        }
        
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
}

void shutdownSockets()
{
    for(unsigned i = 0; i < server_count; ++i) {
    shutdown(servers[i], SHUT_RDWR);
    close(servers[i]);
    }
    delete[] servers;
}

int main(int argc, char **argv) {
	//Print a starting message
	printf("--== Clock Server Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Clock Server Initializing ...\n");
	
	parseArguments(argc, argv);
	
	loadConfigFile();
	
	initialize();
	////////////////////////////////////////////////////

    cout << "Waiting for region server connections" << flush;

    acceptRegionServers();

    cout << " All region servers connected!" << endl;

    run();

    ////////////////////////////////////////////////////
    
    shutdownSockets();


    return 0;
}
