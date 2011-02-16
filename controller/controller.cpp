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

#include "../common/clientrobot.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/timestep.pb.h"
#include "../common/worldinfo.pb.h"
#include "../common/claimteam.pb.h"
#include "../common/claim.pb.h"

#include "../common/ports.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/parseconf.h"
#include "../common/net.h"
#include "../common/except.h"
#include "../common/helper.h"

#define ROBOT_LOOKUP_SIZE 100

using namespace std;


class ClientConnection : public net::EpollConnection {
public:
	size_t id;

	ClientConnection(int id_, int epoll, int flags, int fd, Type type) : net::EpollConnection(epoll, flags, fd, type), id(id_) {}
};


struct lookup_pair {
  net::EpollConnection *server;
  net::EpollConnection *client;
};

lookup_pair robots[ROBOT_LOOKUP_SIZE];

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

int main(int argc, char** argv)
{
  size_t clientcount = 0;
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
  net::EpollConnection clockconn(epoll, EPOLLIN, clockfd, net::connection::CLOCK),
    listenconn(epoll, EPOLLIN, listenfd, net::connection::CLIENT_LISTEN);

  TimestepUpdate timestep;
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  ClientRobot clientrobot;
  ServerRobot serverrobot;
  ClaimTeam claimteam;
  Claim claimrobot;
  vector<ClientConnection*> clients;

  #define MAX_EVENTS 128
  struct epoll_event events[MAX_EVENTS];
  while(true) {
    int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
    if(0 > eventcount) {
      perror("Failed to wait on sockets");
      break;
    }

    for(size_t i = 0; i < (unsigned)eventcount; ++i) {
      net::EpollConnection *c = (net::EpollConnection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case net::connection::CLOCK:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_WORLDINFO:
              worldinfo.ParseFromArray(buffer, len);
              cout << "Got world info." << endl;
              break;

            case MSG_REGIONINFO:
							//should add connections to region servers
              regioninfo.ParseFromArray(buffer, len);
              cout << "Got region info." << endl;
              break;

            case MSG_TIMESTEPUPDATE:
              timestep.ParseFromArray(buffer, len);

              // Enqueue update to all clients
              for(vector<ClientConnection*>::iterator i = clients.begin();
                  i != clients.end(); ++i) {
                (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
                (*i)->set_writing(true);
              }
              break;

            case MSG_CLAIMTEAM:
            {
              claimteam.ParseFromArray(buffer, len);
              unsigned client = claimteam.clientid();
              if(claimteam.granted()) {
                cout << "Client " << client << " granted team " << claimteam.id()
                     << "." << endl;
                // TODO: Send list of robots to client
                // TODO: Update lookup table
              } else {
                cout << "Client " << client << "'s request for team "
                     << claimteam.id() << " was rejected." << endl;
                // Notify client of rejection
                claimteam.clear_clientid();
                clients[client]->queue.push(MSG_CLAIMTEAM, claimteam);
                clients[client]->set_writing(true);
              }
              break;
            }

            default:
              cerr << "Unexpected readable socket!" << endl;
            }
          }
          break;
        }
        
				case net::connection::CLIENT:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_CLIENTROBOT:
            {
              // Forward to the correct server
              clientrobot.ParseFromArray(buffer, len);
              net::EpollConnection *server = robots[clientrobot.id()].server;
              cout << "Received client robot with ID #" << clientrobot.id()
                   << endl;
              server->queue.push(MSG_CLIENTROBOT, clientrobot);
              server->set_writing(true);
              break;
            }

            case MSG_CLAIMTEAM:
              // Forward to the clock
              claimteam.ParseFromArray(buffer, len);
              clockconn.queue.push(MSG_CLAIMTEAM, claimteam);
              clockconn.set_writing(true);
              break;

            default:
              cerr << "Unexpected readable socket from client!" << endl;
              break;
            }
          }
					break;
				}
				
				case net::connection::REGION:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_SERVERROBOT:
            {
              serverrobot.ParseFromArray(buffer, len);
              // Forward to the correct client
              net::EpollConnection *client = robots[serverrobot.id()].server;
              cout << "Received server robot with ID #" << serverrobot.id()
                   << endl;
              client->queue.push(MSG_SERVERROBOT, serverrobot);
              client->set_writing(true);
              break;
            }

            case MSG_CLAIM:
              // Update lookup table
              claimrobot.ParseFromArray(buffer, len);
              robots[claimrobot.id()].server = c;
              break;

            default:
              cerr << "Unexpected readable socket from client!" << endl;
              break;
            }
          }
					break;
				}
				
        case net::connection::CLIENT_LISTEN:
				//for client connections
        {
          int fd = accept(c->fd, NULL, NULL);
          if(fd < 0) {
            throw SystemError("Failed to accept client");
          }
          net::set_blocking(fd, false);

          ClientConnection *newconn = new ClientConnection(clientcount++, epoll, EPOLLIN, fd, net::connection::CLIENT);
          clients.push_back(newconn);

          // Pass WorldInfo to client so it can calculate how many robots
          // per team, and how many teams there are.
          newconn->queue.push(MSG_WORLDINFO, worldinfo);
          newconn->set_writing(true);

          break;
        }
        default:
          cerr << "Unexpected readable socket!" << endl;
        }
      } else if(events[i].events & EPOLLOUT) {
				//ready to write
        switch(c->type) {
        case net::connection::CLIENT:
        {
          if(c->queue.doWrite()) {
            c->set_writing(false);
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
