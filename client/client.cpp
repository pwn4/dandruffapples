/*/////////////////////////////////////////////////////////////////////////////////////////////////
Client program
This program communications with controllers.
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
#include "../common/net.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/except.h"

using namespace std;

/////////////////Variables and Declarations/////////////////
char configFileName [30] = "config";

//Config variables
char controllerip [10][40] = new char[10][40]; //controller IPs - max 10. Don't really need any more.
int freecontroller = 0;

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
			if(strcmp(token, "CONTROLLERIP") == 0){
				token = strtok(NULL, " \n");
				strcpy(controllerip[freeController++], token);
				printf("Storing controller IP: %s\n", clockip);
			}
			
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}


void run() {
  int controllerfd = -1;
  int currentController = rand() % freeController;
  while(controllerfd < 0)
  {
    cout << "Attempting to connect to controller " <<  << "..." << flush;
    controllerfd = net::do_connect(controllerip[i], CLIENT_PORT);
    if(0 > controllerfd) {
      cout << " failed to connect." << endl;
    } else if(0 == controllerfd) {
      cerr << " invalid address: " << controllerfd << endl;
    }
    currentController = rand() % freeController;
  }

  cout << " connected." << endl;

  TimestepUpdate timestep;
  MessageReader reader(controllerfd);
  MessageType type;
  size_t len;
  const void *buffer;
/*
  try {
    cout << "Notifying clock server that our init's complete..." << flush;
    while(true) {
      for(bool complete = false; !complete;) {
        complete = writer.doWrite();
      }
      cout << " done." << endl;
      
      cout << "Waiting for timestep..." << flush;
      for(bool complete = false; !complete;) {
        complete = reader.doRead(&type, &len, &buffer);
      }
      timestep.ParseFromArray(buffer, len);
      cout << " got " << timestep.timestep() << ", replying..." << flush;
    }
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }*/

  // Clean up
  shutdown(controllerfd, SHUT_RDWR);
  close(controllerfd);
}

//this is the main loop for the client
int main(int argc, char* argv[])
{
	//Print a starting message
	printf("--== Client Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Client Initializing ...\n");
	
	parseArguments(argc, argv);
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Client Running!\n");
	
	run();
	
	printf("Client Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
