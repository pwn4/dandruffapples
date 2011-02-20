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

#define ROBOT_LOOKUP_SIZE 250000

using namespace std;


class ClientConnection : public net::EpollConnection {
public:
	size_t id;

	ClientConnection(int id_, int epoll, int flags, int fd, Type type) : net::EpollConnection(epoll, flags, fd, type), id(id_) {}
};

class ServerConnection : public net::EpollConnection {
public:
	size_t id;

	ServerConnection(int id_, int epoll, int flags, int fd, Type type) : net::EpollConnection(epoll, flags, fd, type), id(id_) {}
};


struct lookup_pair {
  net::EpollConnection *server;
  net::EpollConnection *client;
};

lookup_pair robots[ROBOT_LOOKUP_SIZE];

struct NoTeamRobot {
  int robotId;
  int regionId;
  int teamId;
  
  NoTeamRobot(int robotId_, int regionId_, int teamId_) : robotId(robotId_),
     regionId(regionId_), teamId(teamId_) {}
};

vector<NoTeamRobot> unassignedRobots;


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
  vector<ServerConnection*> servers;

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
          int len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_WORLDINFO:
            {
              worldinfo.ParseFromArray(buffer, len);
              cout << "Got world info." << endl;
              
              // Get the robot information.
              for (int i = 0; i < worldinfo.robot_size(); i++) {
                unassignedRobots.push_back(NoTeamRobot(worldinfo.robot(i).id(),
                    worldinfo.robot(i).region(), worldinfo.robot(i).team()));
              }

              // Get the RegionServer connections.
              for (int i = 0; i < worldinfo.region_size(); i++) {
                struct in_addr addr;
                addr.s_addr = worldinfo.region(i).address();
                int regionfd = net::do_connect(addr,
                    worldinfo.region(i).controllerport());
                if (regionfd < 0) {
                  cout << "Failed to connect to a Region Server.\n";
                } else if (regionfd == 0) {
                  cout << "Invalid Region Server address\n";
                } else {
                  cout << "Connected to a Region Server" << endl;
                }
                ServerConnection *newServer = new ServerConnection(
                    worldinfo.region(i).id(), epoll, EPOLLIN, regionfd, 
                    net::connection::REGION);
                servers.push_back(newServer);

                // Populate lookup table.
                for(vector<NoTeamRobot>::iterator j = unassignedRobots.begin();
                    j != unassignedRobots.end(); ++j) {
                  if (j->regionId == (int)newServer->id) {
                    robots[j->robotId].server = newServer;
                    robots[j->robotId].client = NULL;
                  }
                }
              }


              break;
            }

            case MSG_REGIONINFO:
            {
              // TODO: Make a function perform the identical code here and in
              //       the MSG_WORLDINFO case.
              regioninfo.ParseFromArray(buffer, len);
              cout << "Got a new RegionInfo message! Trying to connect...\n";
              struct in_addr addr;
              addr.s_addr = regioninfo.address();
              int regionfd = net::do_connect(addr, regioninfo.controllerport());
              if (regionfd < 0) {
                cout << "Failed to connect to a Region Server.\n";
              } else if (regionfd == 0) {
                cout << "Invalid Region Server address\n";
              } else {
                cout << "Connected to a Region Server" << endl;
              }
              ServerConnection *newServer = new ServerConnection(
                  regioninfo.id(), epoll, EPOLLIN, regionfd, 
                  net::connection::REGION);
              servers.push_back(newServer);

              // Populate lookup table.
              for(vector<NoTeamRobot>::iterator i = unassignedRobots.begin();
                  i != unassignedRobots.end(); ++i) {
                if (i->regionId == (int)newServer->id) {
                  robots[i->robotId].server = newServer;
                }
              }
              break;
            }

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

                // Update lookup table. 
                for(vector<NoTeamRobot>::iterator i = unassignedRobots.begin();
                    i != unassignedRobots.end(); ++i) {
                  if (i->teamId == (int)claimteam.id()) {
                    robots[i->robotId].client = clients[client];
                    cout << "Assigned robot " << i->robotId << " to client "
                         << client << endl;
                    // TODO: Erase unassignedRobot entries when done.
                  }
                }
              } else {
                cout << "Client " << client << "'s request for team "
                     << claimteam.id() << " was rejected." << endl;
              }
              // Notify client of acceptance/rejection
              claimteam.clear_clientid();
              clients[client]->queue.push(MSG_CLAIMTEAM, claimteam);
              clients[client]->set_writing(true);

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
          int len;
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
            {
              // Forward to the clock
              claimteam.ParseFromArray(buffer, len);
              claimteam.set_clientid(((ClientConnection*)c)->id);

              clockconn.queue.push(MSG_CLAIMTEAM, claimteam);
              clockconn.set_writing(true);
              break;
            }

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
          int len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_SERVERROBOT:
            {
              serverrobot.ParseFromArray(buffer, len);
              // Forward to the correct client
              net::EpollConnection *client = robots[serverrobot.id()].client;
              cout << "Received server robot with ID #" << serverrobot.id()
                   << endl;
              if (client == NULL) {
                cout << "No client has claimed this robot yet!\n";
              } else {
                client->queue.push(MSG_SERVERROBOT, serverrobot);
                client->set_writing(true);
              }
              break;
            }

            case MSG_CLAIM:
              // Update lookup table
              claimrobot.ParseFromArray(buffer, len);
              robots[claimrobot.id()].server = c;
              cout << "Do we get claims? Robot ID #" << claimrobot.id()
                   << endl;
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
          // Fall through...
        case net::connection::CLOCK:
          // Sending ClaimTeam messages
          // Fall through...
        case net::connection::REGION:
          if(c->queue.doWrite()) {
            c->set_writing(false);
          }
          break;
        default:
          cerr << "Unexpected writable socket of type " << c->type << "!" 
               << endl;
          break;
        }
      }
    }
  }
}
