#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <queue>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <google/protobuf/message_lite.h>

#include "../common/ports.h"
#include "../common/timestep.pb.h"
#include "../common/messagereader.h"
#include "../common/messagewriter.h"
#include "../common/net.h"
#include "../common/except.h"

#define ROBOT_LOOKUP_SIZE 100

using namespace std;

struct server_client{
	int *server;
	int *client;
};

//variable declarations
server_client lookup[ROBOT_LOOKUP_SIZE];	//robot lookup table
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

struct connection {
  enum Type {
    UNSPECIFIED,
    LISTEN,
    CLOCK,
    CLIENT,
    REGION
  } type;
  int fd;
  MessageReader reader;
  MessageWriter writer;
  queue<google::protobuf::MessageLite*> waiting;

  connection(int fd_) : type(UNSPECIFIED), fd(fd_), reader(fd_), writer(fd_) {}
  connection(int fd_, Type type_) : type(type_), fd(fd_), reader(fd_), writer(fd_) {}
};

int main(/*int argc, char* argv[]*/)
{
	// Print a starting message
	printf("--== Controller Server Software ==-\n");
	
	clockfd = net::do_connect(clockip, CONTROLLERS_PORT);
	cout << "Connected to Clock Server" << endl;
    
  listenfd = net::do_listen(CLIENTS_PORT);
  net::set_blocking(listenfd, false);

  int epoll = epoll_create(1000);

  // Add clock and client sockets to epoll
  connection clockconn(clockfd, connection::CLOCK),
    listenconn(listenfd, connection::LISTEN);
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = &clockconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, clockfd, &event)) {
    perror("Failed to add clock socket to epoll");
    close(clockfd);
    close(listenfd);
    return 1;
  }
  event.data.ptr = &listenconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, listenfd, &event)) {
    perror("Failed to add listen socket to epoll");
    close(clockfd);
    close(listenfd);
    return 1;
  }

  TimestepUpdate timestep;
  MessageReader reader(clockfd);
  vector<connection*> clients;

  #define MAX_EVENTS 128
  struct epoll_event events[MAX_EVENTS];
  while(true) {
    int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
    if(0 > eventcount) {
      perror("Failed to wait on sockets");
      break;
    }

    for(size_t i = 0; i < (unsigned)eventcount; ++i) {
      connection *c = (connection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case connection::CLOCK:
        {
          size_t len;
          const void *buffer;
          if(c->reader.doRead(NULL, &len, &buffer)) {
            timestep.ParseFromArray(buffer, len);

            // Enqueue update to all clients
            for(vector<connection*>::iterator i = clients.begin();
                i != clients.end(); ++i) {
              (*i)->waiting.push(new TimestepUpdate(timestep));
              event.events = EPOLLIN | EPOLLOUT;
              event.data.ptr = *i;
              epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
            }
          }
          break;
        }
        
        case connection::LISTEN:
        {
          int fd = accept(c->fd, NULL, NULL);
          if(fd < 0) {
            throw SystemError("Failed to accept client");
          }
          net::set_blocking(fd, false);

          connection *newconn = new connection(fd, connection::CLIENT);
          clients.push_back(newconn);

          event.events = EPOLLIN;
          event.data.ptr = newconn;
          if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
            perror("Failed to add client connection to epoll");
            return 1;
          }
          break;
        }
        default:
          cerr << "Unexpected readable socket!" << endl;
          break;
        }
      } else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case connection::CLIENT:
        {
          if(!c->writer.writing()) {
            if(!c->waiting.empty()) {
              c->writer.init(TIMESTEPUPDATE, c->waiting.front());
            }
          }
          if(c->writer.doWrite()) {
            delete c->waiting.front();
            c->waiting.pop();
            if(!c->waiting.empty()) {
              c->writer.init(TIMESTEPUPDATE, c->waiting.front());
            }
          }
          
          break;
        }
        default:
          cerr << "Unexpected writable socket!" << endl;
          break;
        }
      }
    }
  }
}
