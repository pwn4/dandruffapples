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
#include "../common/messagereader.h"
#include "../common/net.h"

#define ROBOT_LOOKUP_SIZE 100

using namespace std;

struct server_client{
	int *server;
	int *client;
};

//variable declarations
server_client lookup[ROBOT_LOOKUP_SIZE];	//robot lookup table
char configFileName [30] = "config";
int servfd, listenfd, clientfd;

//Config variables
char clockip [40] = "127.0.0.1";


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


//Server claims robot
//rid: robot id
//fd:  socket file descriptor
void serverClaim(int rid, int *fd){
	lookup[rid].server = fd;
}

//Client claims robot
//rid: robot id
//fd:  socket file descriptor
void clientClaim(int rid, int *fd){
	lookup[rid].client = fd;
}

int main(/*int argc, char* argv[]*/)
{
	// Print a starting message
	printf("--== Controller Server Software ==-\n");
	
	servfd = net::do_connect(clockip, CONTROLLERS_PORT);
	cout << "Connected to Clock Server" << endl;
    
  listenfd = net::do_listen(CLIENTS_PORT);
  net::set_blocking(listenfd, false);

  TimestepUpdate timestep;
  MessageReader reader(servfd);

  MessageType type;
  size_t len;
  const void *buffer;
  while(true) {
    for(bool complete = false; !complete;) {
      complete = reader.doRead(&type, &len, &buffer);
    }
    timestep.ParseFromArray(buffer, len);
    cout << "Got timestep " << timestep.timestep() << endl;
  }
}
