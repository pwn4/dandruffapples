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
#include <set>

#include "../common/clientrobot.pb.h"
#include "../common/regionupdate.pb.h"
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

struct RobotConnection{
  vector<ClientRobot> bouncedMessages;
  net::EpollConnection* client;
  net::EpollConnection* server;

  RobotConnection(net::EpollConnection* c, net::EpollConnection* s) : client(c), server(s) {}
};

//When looking up a robot id, you can figure out which server and client it belongs to
map<int, RobotConnection> robots;


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
  helper::CmdLine cmdline(argc, argv);
  configFileName=cmdline.getArg("-c", "config" ).c_str();
  loadConfigFile();

  strcpy(clockip, cmdline.getArg("-l", (string)clockip, 40).c_str());

  cout << "Using clock IP: " << clockip << endl;

  // Stat variables
  time_t lastSecond = time(NULL);
  int receivedClientRobot = 0;
  int sentClientRobot = 0;
  int receivedServerRobot = 0;
  int sentServerRobot = 0;

  // Print a starting message
  printf("--== Controller Server Software ==-\n");

  clockfd = net::do_connect(clockip, CONTROLLERS_PORT);

  if (clockfd < 0) {
    throw SystemError("Failed to connect to clock server");
	} else if (clockfd == 0) {
		throw runtime_error("Invalid clock address");
	}
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
  TimestepDone tsdone;
  tsdone.set_done(true);
  vector<ClientConnection*> clients;	//can access by client id
  vector<ServerConnection*> servers;
	set<net::EpollConnection*> seenbyidset;		//to determine who to forward serverrobot msg to
	pair<set<net::EpollConnection*>::iterator,bool> ret; //return value of insertion
  bool flushing = false;

  #define MAX_EVENTS 128
  struct epoll_event events[MAX_EVENTS];
  while(true) {
    // Stats: Check message send rate
    if (lastSecond < time(NULL)) {
      cout << "Received " << receivedClientRobot
           << " ClientRobot messages per second." << endl;
      cout << "Sent " << sentClientRobot
           << " ClientRobot messages per second." << endl;
      cout << "Received " << receivedServerRobot
           << " ServerRobot messages per second." << endl;
      cout << "Sent " << sentServerRobot
           << " ServerRobot messages per second.\n" << endl;
      lastSecond = time(NULL);
      receivedClientRobot = 0;
      sentClientRobot = 0;
      receivedServerRobot = 0;
      sentServerRobot = 0;
    }

    int eventcount;
    do {
      eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
    } while(eventcount < 0 && errno == EINTR);
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
                  throw SystemError("Failed to connect to a region server");
                } else if (regionfd == 0) {
                  throw runtime_error("Region Server address");
                } else {
                  cout << "Connected to a region server." << endl;
                }
                ServerConnection *newServer = new ServerConnection(
                    worldinfo.region(i).id(), epoll, EPOLLIN, regionfd,
                    net::connection::REGION);
                servers.push_back(newServer);

                // Populate lookup table.
                for(vector<NoTeamRobot>::iterator j = unassignedRobots.begin();
                    j != unassignedRobots.end(); ++j) {
                  if (j->regionId == (int)newServer->id) {
                    robots.insert ( pair<int,RobotConnection>(j->robotId, RobotConnection(NULL, newServer)) );
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
                throw SystemError("Failed to connect to a region server");
              } else if (regionfd == 0) {
                throw runtime_error("Region Server address");
              } else {
                cout << "Connected to a region server." << endl;
              }
              ServerConnection *newServer = new ServerConnection(
                  regioninfo.id(), epoll, EPOLLIN, regionfd,
                  net::connection::REGION);
              servers.push_back(newServer);

              // Populate lookup table.
              for(vector<NoTeamRobot>::iterator i = unassignedRobots.begin();
                  i != unassignedRobots.end(); ++i) {
                if (i->regionId == (int)newServer->id) {
                  robots.insert ( pair<int,RobotConnection>(i->robotId, RobotConnection(NULL, newServer)) );
                }
              }
              break;
            }

            case MSG_TIMESTEPUPDATE:
            {
              // Parsing this is strictly a waste of CPU.
              timestep.ParseFromArray(buffer, len);

              // Begin output buffer flush
              flushing = true;

              // Enqueue update to all clients
							vector<ClientConnection*>::iterator clientsEnd = clients.end();
              for(vector<ClientConnection*>::iterator i = clients.begin();
                  i != clientsEnd; ++i) {
                (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
                (*i)->set_writing(true);
                // No more reading until we've finished flushing
                (*i)->set_reading(false);
              }

              // No more reading until we've finished flushing
							vector<ServerConnection*>::iterator serversEnd = servers.end();
              for(vector<ServerConnection*>::iterator i = servers.begin();
                  i != serversEnd; ++i) {
                (*i)->set_reading(false);
              }
              break;
            }

            case MSG_CLAIMTEAM:
            {
              ClaimTeam claimteam;
              claimteam.ParseFromArray(buffer, len);
              unsigned client = claimteam.clientid();
              if(claimteam.granted()) {
                cout << "Client " << client << " granted team " << claimteam.id()
                     << "." << endl;

                // Update lookup table.
                for(vector<NoTeamRobot>::iterator i = unassignedRobots.begin();
                    i != unassignedRobots.end(); ++i) {
                  if (i->teamId == (int)claimteam.id()) {

                    if(robots.find(i->robotId) == robots.end())
                      throw runtime_error("No server known for robot.");

                    (robots.find(i->robotId))->second.client = clients[client];
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
              break;
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
              receivedClientRobot++;
              // Forward to the correct server
              ClientRobot clientrobot;
              clientrobot.ParseFromArray(buffer, len);

              net::EpollConnection *server = (robots.find(clientrobot.id()))->second.server;
							if(server == NULL){//this should not happen...
								cout << "Ugh, robot#" << clientrobot.id() << "does not belong to a server?"
										 << endl;
							}
							else{
		            server->queue.push(MSG_CLIENTROBOT, clientrobot);
		            server->set_writing(true);
                sentClientRobot++;
							}
              break;
            }

            case MSG_CLAIMTEAM:
            {
              ClaimTeam claimteam;
              // Forward to the clock
              claimteam.ParseFromArray(buffer, len);

              //temporary error for now to ensure this isn't begin wonky - so, limit is 1000 clients per controller
              if(((ClientConnection*)c)->id >= 1000)
                throw runtime_error("Invalid Client Connection Id Due to Bad Cast");

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
              ServerRobot serverrobot;
              receivedServerRobot++;
              serverrobot.ParseFromArray(buffer, len);

							seenbyidset.clear();	//clear for next serverrobot msg

							// Forward to the client, and any clients in the seenbyid
              net::EpollConnection *client = (robots.find(serverrobot.id()))->second.client;
              if (client != NULL) {
								ret = seenbyidset.insert(client);
								if(ret.second){
		              client->queue.push(MSG_SERVERROBOT, serverrobot);
		              client->set_writing(true);
	                sentServerRobot++;
								}
              }
							//lookup clients using seenbyid and store into set then send to all clients
							//this resolves sending multiple msgs to same client.
							int seenbyidsize = serverrobot.seesserverrobot_size();
							for(int i=0; i<seenbyidsize; i++){
							  client = (robots.find(serverrobot.seesserverrobot(i).seenbyid()))->second.client;
                if (client != NULL) {
                  ret = seenbyidset.insert(client);
									if(ret.second){
										client->queue.push(MSG_SERVERROBOT, serverrobot);
				            client->set_writing(true);
			              sentServerRobot++;
									}
                }
							}

              break;
            }

            case MSG_PUCKSTACK:
            {
              PuckStack puckstack;
              puckstack.ParseFromArray(buffer, len);

              seenbyidset.clear();
              net::EpollConnection *client;
							int seenbyidsize = puckstack.seespuckstack_size();
              for(int i=0; i<seenbyidsize; i++){
                client = (robots.find(puckstack.seespuckstack(i).seenbyid()))->
                    second.client;
                if (client != NULL) {
                  ret = seenbyidset.insert(client);
                  if(ret.second){
                    client->queue.push(MSG_PUCKSTACK, puckstack);
                    client->set_writing(true);
                  }
                }
              }

              break;
            }

            case MSG_CLAIM:
            {
              // Update lookup table
              Claim claimrobot;
              claimrobot.ParseFromArray(buffer, len);
              //another temporary error handle in case
              if(robots.find(claimrobot.id()) == robots.end())
                throw runtime_error("Could not find claimrobot id in robot array upon region claim.");

              (robots.find(claimrobot.id()))->second.server = c;

              //we check to see if the robot has backed up bounced messages
              if((robots.find(claimrobot.id()))->second.bouncedMessages.size() > 0)
              {
                net::EpollConnection* robotServer = (robots.find(claimrobot.id()))->second.server;
                vector<ClientRobot> * robotBacklog = &((robots.find(claimrobot.id()))->second.bouncedMessages);
                for(unsigned int i = 0; i < robotBacklog->size(); i++)
                {
                  robotServer->queue.push(MSG_CLIENTROBOT, (*robotBacklog)[i]);
		              robotServer->set_writing(true);
	              }

		            //force flush the message before we clear the backlog (OR ELSE)
                while(robotServer->queue.remaining() != 0)
                  robotServer->queue.doWrite();

                //NOW we can clear the backlog
                robotBacklog->clear();
              }

              break;
            }

            case MSG_BOUNCEDROBOT:
            {
              // 1 bounce, try again. 2 bounces, broadcast
              BouncedRobot bouncedrobot;
              bouncedrobot.ParseFromArray(buffer, len);
              if (bouncedrobot.bounces() == 1) {

                //another temporary error handle in case
                if(robots.find(bouncedrobot.clientrobot().id()) == robots.end())
                  throw runtime_error("Could not find claimrobot id in robot array upon bounced message.");

                // Store it
                (robots.find(bouncedrobot.clientrobot().id()))->second.bouncedMessages.push_back(bouncedrobot.clientrobot());
                //sentClientRobot++;
              }

              //see if we need to flush the buffer
              //we check to see if the robot has backed up bounced messages
              if((robots.find(bouncedrobot.clientrobot().id()))->second.server != c){
                //then the server has changed, so
                if((robots.find(bouncedrobot.clientrobot().id()))->second.bouncedMessages.size() > 0)
                {
                  net::EpollConnection* robotServer = (robots.find(bouncedrobot.clientrobot().id()))->second.server;
                  vector<ClientRobot> * robotBacklog = &((robots.find(bouncedrobot.clientrobot().id()))->second.bouncedMessages);
									unsigned int robotBacklogSize = robotBacklog->size();
                  for(unsigned int i = 0; i < robotBacklogSize; i++)
                  {
                    robotServer->queue.push(MSG_CLIENTROBOT, (*robotBacklog)[i]);
		                robotServer->set_writing(true);
	                }

		              //force flush the message before we clear the backlog (OR ELSE)
                  while(robotServer->queue.remaining() != 0)
                    robotServer->queue.doWrite();

                  //NOW we can clear the backlog
                  robotBacklog->clear();
                }
              }

              break;
            }
            default:
              cerr << "Unexpected readable socket from region!" << endl;
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
          break;
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
          if(flushing) {
            bool incomplete = false;

            // Check for remaining data to write
						vector<ServerConnection*>::iterator serversEnd = servers.end();
						vector<ClientConnection*>::iterator clientsEnd = clients.end();
            for(vector<ServerConnection*>::iterator i = servers.begin();
                i != serversEnd; ++i) {
              if(incomplete) {
                break;
              }
              incomplete = (*i)->queue.remaining();
            }
            for(vector<ClientConnection*>::iterator i = clients.begin();
                i != clientsEnd; ++i) {
              if(incomplete) {
                break;
              }
              incomplete = (*i)->queue.remaining();
            }
            if(incomplete) {
              break;
            }
            // Flush completed; Tell the clock we're done and resume reading
            flushing = false;
            clockconn.queue.push(MSG_TIMESTEPDONE, tsdone);
            clockconn.set_writing(true);
            for(vector<ServerConnection*>::iterator i = servers.begin();
                i != serversEnd; ++i) {
              (*i)->set_reading(true);
            }
            for(vector<ClientConnection*>::iterator i = clients.begin();
                i != clientsEnd; ++i) {
              (*i)->set_reading(true);
            }
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

  return 0;
}
