/*/////////////////////////////////////////////////////////////////////////////////////////////////
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
#include "../common/net.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/except.h"

using namespace std;

//variable declarations
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

int main(int argc, char* argv[])
{
	parseArguments(argc, argv);
    loadConfigFile();
    servfd = net::do_connect(clockip, PNG_VIEWER_PORT);
    cout  << "serverfd:" << servfd;
    cout << "Connected to Clock Server" << endl;
    
}