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

#include "../common/helper.h"

using namespace std;

/////////////////Variables and Declarations/////////////////
const char *configFileName;

//Config variables
vector<string> controllerips; //controller IPs 

////////////////////////////////////////////////////////////

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
				string newcontrollerip = token;
				controllerips.push_back(newcontrollerip);
				printf("Storing controller IP: %s\n", newcontrollerip.c_str());
			}
			
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}


void run() {
  int controllerfd = -1;
  int currentController = rand() % controllerips.size();
  while(controllerfd < 0)
  {
    cout << "Attempting to connect to controller " << controllerips.at(currentController) << "..." << flush;
    controllerfd = net::do_connect(controllerips.at(currentController).c_str(), CLIENTS_PORT);
    if(0 > controllerfd) {
      cout << " failed to connect." << endl;
    } else if(0 == controllerfd) {
      cerr << " invalid address: " << controllerfd << endl;
    }
    currentController = rand() % controllerips.size();
  }

  cout << " connected." << endl;

  TimestepUpdate timestep;
  MessageReader reader(controllerfd);
  MessageType type;
  size_t len;
  const void *buffer;

  // Create thread: client robot calculations
  // parent thread: continue in while loop, looking for updates

  try {
    //cout << "Notifying clock server that our init's complete..." << flush;
    while(true) {
      //for(bool complete = false; !complete;) {
      //  complete = writer.doWrite();
      //}
      //cout << " done." << endl;
      
      //cout << "Waiting for timestep..." << flush;
      for(bool complete = false; !complete;) {
        complete = reader.doRead(&type, &len, &buffer);
      }

      switch (type) {
      case MSG_TIMESTEPUPDATE:
        timestep.ParseFromArray(buffer, len);
        cout << "Client received timestep #" << timestep.timestep() << endl; 
        // update timestep global variable
        break;
      case MSG_SERVERROBOT:
        cout << "Received ServerRobot update!" << endl;
        // update robot position global variables
        break;
      default:
        cout << "Unknown message!" << endl;
        break;
      }
    }
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }

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
	
	helper::Config config(argc, argv);
	configFileName=config.getArg("-c").c_str();
	cout<<"Using config file: "<<configFileName<<endl;
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Client Running!\n");
	
	run();
	
	printf("Client Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
