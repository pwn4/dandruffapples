/*/////////////////////////////////////////////////////////////////////////////////////////////////
Regionserver program
This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
using namespace std;

/////////////////Variables and Declarations/////////////////

char configFileName [30];

//Config variables - MUST BE SET IN THE CONFIG FILE
double regionWidth, regionHeight;

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
			token = strtok(readBuffer, " ");
			
			//if it's a REGION WIDTH definition...
			if(strcmp(token, "REGIONW") == 0){
				token = strtok(NULL, " ");
				regionWidth = atof(token);
			}
			
			//if it's a REGION HEIGHT definition...
			if(strcmp(token, "REGIONH") == 0){
				token = strtok(NULL, " ");
				regionHeight = atof(token);
			}
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file\n");
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
	
	printf("Server Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
