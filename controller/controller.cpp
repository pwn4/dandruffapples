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
	//Print a starting message
	printf("--== Controller Server Software ==-\n");

	//connect to clock server

	unsigned int sock_len = sizeof(struct sockaddr_in);
    
    struct sockaddr_in clientaddr;
    
    //using net::functions instead of manually connecting. comment for now, delete later if correctly not needed
    
    //struct sockaddr_in servaddr, clientaddr, cntraddr;
	/*if ( (servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
	}

	//fill in the servaddr fields
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(CLOCK_PORT);
	if(inet_pton(AF_INET, clockip, &servaddr.sin_addr) <= 0) {		//hardcoded
		printf("inet_pton error for localhost\n");
	}

	if(connect(servfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
	//connect to the server
		printf("connect error\n");
	}
	else{
		serverClaim(1,&servfd);
	}*/
	
	servfd = net::do_connect(clockip, CONTROLLERS_PORT);
	
	cout << "Connected to Clock Server" << endl << flush;

	//clockserver and regionserver connections successful!

	//ready to receive client connections!
    /*listenfd = socket(AF_INET, SOCK_STREAM, 0);

	//fill out the cntraddr fields
    bzero(&cntraddr, sizeof(cntraddr));
    cntraddr.sin_family = AF_INET;
    cntraddr.sin_addr.s_addr = htonl(INADDR_ANY);	//receives packets from all interfaces
    cntraddr.sin_port = htons(CONTROLLERS_PORT);

    bind(listenfd, (struct sockaddr *) &cntraddr, sizeof(cntraddr));
    listen(listenfd, 1000);							*/
    
    listenfd = net::do_listen(CLIENTS_PORT, false);
    

  TimestepUpdate timestep;

  int bufsize=1024;        /* a 1K socket input buffer. May need to increase later. */
  int recvsize;
  char *buffer = new char[bufsize];
  std::string packetBuffer="";
  protoPacket nextPacket;

	while(1){

    //check for data from the clock server
    recvsize = recv(servfd,buffer,bufsize,0);
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
            }
            
            //don't do anything if its of a different type. Will add more later for different proto types
            

            nextPacket = parsePacket(&packetBuffer);
        }

        //get more data
        recvsize = recv(servfd,buffer,bufsize,0);
    }
	
	
		clientfd = accept(listenfd, (struct sockaddr *) &clientaddr, &sock_len);
		pid_t pid;

		if( (pid = fork()) == 0 ){
			//client has connected!
			close(listenfd);
			    //printf("Got connection from: %s pid:%d\n",inet_ntoa(clientaddr.sin_addr), getpid());
			close(clientfd);
			exit(0);
		}
		close(clientfd);
	}
}
