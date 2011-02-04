#include <netinet/in.h>
#include <time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../common/ports.h"
#include "../common/timestep.pb.h"

#define ROBOT_LOOKUP_SIZE 100

using namespace std;

struct server_client{
	int *server;
	int *client;
};

server_client lookup[ROBOT_LOOKUP_SIZE];	//robot lookup table

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
	//connect to clock server
	int	servfd, listenfd, clientfd;
	unsigned int sock_len = sizeof(struct sockaddr_in);
    struct sockaddr_in servaddr, clientaddr, cntraddr;

	if ( (servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
	}

	//fill in the servaddr fields
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(CLOCK_PORT);
	if(inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {		//hardcoded
		printf("inet_pton error for localhost\n");
	}

	if(connect(servfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
	//connect to the server
		printf("connect error\n");
	}
	else{
		serverClaim(1,&servfd);
	}

	//clockserver and regionserver connections successful!

	//ready to receive client connections!
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

	//fill out the cntraddr fields
    bzero(&cntraddr, sizeof(cntraddr));
    cntraddr.sin_family = AF_INET;
    cntraddr.sin_addr.s_addr = htonl(INADDR_ANY);	//receives packets from all interfaces
    cntraddr.sin_port = htons(CONTROLLERS_PORT);

    bind(listenfd, (struct sockaddr *) &cntraddr, sizeof(cntraddr));
    listen(listenfd, 1000);							//hardcoded

	while(1){
		clientfd = accept(listenfd, (struct sockaddr *) &clientaddr, &sock_len);
		pid_t pid;

		if( (pid = fork()) == 0 ){
			//client has connected!
			close(listenfd);
			printf("Got connection from: %s%d\n",inet_ntoa(clientaddr.sin_addr), getpid());
			close(clientfd);
			exit(0);
		}
		close(clientfd);
	}
}
