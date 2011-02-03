/*/////////////////////////////////////////////////////////////////////////////////////////////////
Regionserver program
This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "net.h"

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
	
	net::run(clockip);
	
	printf("Server Running!\n");
	
	printf("Server Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
