#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

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

#include "../common/clientrobot.pb.h"
#include "../common/timestep.pb.h"
#include "../common/worldinfo.pb.h"


#include "../common/ports.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/parseconf.h"
#include "../common/net.h"
#include "../common/except.h"
#include "../common/helper.h"

#define ROBOT_LOOKUP_SIZE 100

using namespace std;

struct server_client{
	int *server;
	int *client;
};

//variable declarations
server_client lookup[ROBOT_LOOKUP_SIZE];	//robot lookup table
const char *configFileName;
int clockfd, listenfd, clientfd;

//Config variables
char clockip [40] = "127.0.0.1";

//this function loads the config file so that the server parameters don't need to be added every time
void loadConfigFile()
{
	conf configuration = parseconf(configFileName);

	//clock ip address
	if (configuration.find("CLOCKIP") == configuration.end()) {
		cerr << "Config file is missing an entry!" << endl;
		exit(1);
	}
	strcpy(clockip, configuration["CLOCKIP"].c_str());
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
  MessageQueue queue;

  connection(int fd_) : type(UNSPECIFIED), fd(fd_), reader(fd_), queue(fd_) {}
  connection(int fd_, Type type_) : type(type_), fd(fd_), reader(fd_), queue(fd_) {}
};

int main(int argc, char** argv)
{
  helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	loadConfigFile();

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
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  ClientRobot clientrobot;
//  MessageReader reader(clockfd);
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
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_WORLDINFO:
            {
              worldinfo.ParseFromArray(buffer, len);
              cout << "Got world info." << endl;
              break;
            }
            case MSG_REGIONINFO:
            {
							//should add connections to region servers
              regioninfo.ParseFromArray(buffer, len);
              cout << "Got region info." << endl;
              break;
            }
            case MSG_TIMESTEPUPDATE:
            {
              timestep.ParseFromArray(buffer, len);

              // Enqueue update to all clients
              for(vector<connection*>::iterator i = clients.begin();
                  i != clients.end(); ++i) {
                (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
                event.events = EPOLLIN | EPOLLOUT;
                event.data.ptr = *i;
                epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
              }
              break;
            }
            default:
              cerr << "Unexpected readable socket!" << endl;
            }
          }
          break;
        }
        
				case connection::CLIENT:
				//client is sending clientRobot instructions
        {
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_CLIENTROBOT:
              clientrobot.ParseFromArray(buffer, len);
              cout << "Received client robot with ID #" << clientrobot.id() 
                   << endl;
              break;
            default:
              cerr << "Unexpected readable socket from client!" << endl;
              break;
            }
          }
					break;
				}
				
				case connection::REGION:
				//region servers are sending serverRobot instructions
        {
					break;
				}
				
        case connection::LISTEN:
				//for client connections
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
					close(c->fd);	//supposed to handle when clients disconnect
          cerr << "Unexpected readable socket!, Did client disconnect?" << endl;
          break;
        }
      } else if(events[i].events & EPOLLOUT) {
				//ready to write
        switch(c->type) {
        case connection::CLIENT:
        {
          if(c->queue.doWrite()) {
            event.events = EPOLLIN;
            event.data.ptr = c;
            epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
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
