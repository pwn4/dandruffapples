/*/////////////////////////////////////////////////////////////////////////////////////////////////
Regionserver program
This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/
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

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "../common/ports.h"
#include "../common/timestep.pb.h"
#include "../common/functions.h"
#include "../common/net.h"

using namespace std;

/////////////////Variables and Declarations/////////////////

char configFileName [30] = "config";

//Config variables - MUST BE SET IN THE CONFIG FILE
char clockip [40];

////////////////////////////////////////////////////////////


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
			if(strcmp(token, "CLOCKIP") == 0){
				token = strtok(NULL, " \n");
				strcpy(clockip, token);
				printf("Using clockserver IP: %s\n", clockip);
			}
			
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}

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

void run() {
    int clockfd = net::do_connect(clockip, CLOCK_PORT);
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


//this is the main loop for the server
int main(int argc, char* argv[])
{
	//Print a starting message
	printf("--== Region Server Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Server Initializing ...\n");
	
	parseArguments(argc, argv);
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Server Running!\n");
	
	run();
	
	printf("Server Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
