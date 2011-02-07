/*////////////////////////////////////////////////////////////////////////////////////////////////
 PNGViewer program
 This program communications with clock servers and region servers
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
#include "../common/worldinfo.pb.h"
#include "../common/net.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/except.h"

using namespace std;

//variable declarations
char configFileName [30] = "config";
int clockfd, listenfd, clientfd;

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

char* IPAddressToString(int ip)
{
  char* addr = new char[16];

  sprintf(addr, "%d.%d.%d.%d",
    (ip >> 24) & 0xFF,
    (ip >> 16) & 0xFF,
    (ip >>  8) & 0xFF,
    (ip      ) & 0xFF);
  return addr;
}


int main(int argc, char* argv[])
{
	parseArguments(argc, argv);
  loadConfigFile();
  clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
  cout << "Connected to Clock Server" << endl;

	TimestepUpdate timestep;
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  MessageReader reader(clockfd);
  MessageType type;
  size_t len;
  const void *buffer;

  try {
		while(true) {
		  for(bool complete = false; !complete;) {
		    complete = reader.doRead(&type, &len, &buffer);
		  }
		  switch (type) {
				case MSG_REGIONINFO:
				{
					regioninfo.ParseFromArray(buffer, len);
					cout << "Received MSG_REGIONINFO update!" << regioninfo.address() << " " << regioninfo.port() << endl;
					struct in_addr addr;
//					int fd = net::do_connect(IPAddressToString(ntohl(regioninfo.address())), regioninfo.port());
					cout << "Connected to region server!" << endl;
					break;
				}
				case MSG_WORLDINFO:
				{
					cout << "Received MSG_WORLDINFO update!" << endl;
					break;
				}
				default:
				{
				  cout << "Unknown message!" << endl;
				  break;
				}
		  }
		}
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }   
}
