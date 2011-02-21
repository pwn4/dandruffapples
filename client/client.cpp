/*/////////////////////////////////////////////////////////////////////////////////////////////////
Client program
This program communications with controllers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <math.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "../common/claimteam.pb.h"
#include "../common/clientrobot.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/timestep.pb.h"
#include "../common/worldinfo.pb.h"

#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagequeue.h"
#include "../common/messagereader.h"
#include "../common/except.h"

#include "../common/helper.h"

using namespace std;

/////////////////Variables and Declarations/////////////////
const char *configFileName;

//Game world variables
// TODO: organize/move variables out of client.cpp 
bool simulationStarted = false;
int currentTimestep = 0;
int firstTeam; // lowest teamid we control
int numTeams; // this client computer controls this number of teams.
int robotsPerTeam; 
net::EpollConnection* theController;
int epoll;

class Robot {
public:
  float x;
  float y;
  float vx;
  float vy;
  float angle;
  bool hasPuck;
  bool hasCollided;  

  Robot() : x(0.0), y(0.0), vx(0.0), vy(0.0), angle(0.0), hasPuck(false),
            hasCollided(false) {}
};

class OwnRobot : public Robot {
public:
  bool pendingCommand;

  OwnRobot() : Robot(), pendingCommand(false) {}
};

OwnRobot** ownRobots;

class EnemyRobot : public Robot {
public:
  int id;
  int lastTimestepSeen;

  EnemyRobot() : Robot(), id(-1), lastTimestepSeen(-1) {}
};

// TODO: Change to a hash table or something
vector<EnemyRobot*>* enemyRobots;

//Config variables
vector<string> controllerips; //controller IPs 

////////////////////////////////////////////////////////////

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
			if(strcmp(token, "CONTROLLERIP") == 0){
				token = strtok(NULL, " \n");
				string newcontrollerip = token;
				controllerips.push_back(newcontrollerip);
				printf("Storing controller IP: %s\n", newcontrollerip.c_str());
			} else if (strcmp(token, "FIRSTTEAM") == 0) {
				token = strtok(NULL, " \n");
        firstTeam = strtol(token, NULL, 10);
        cout << "First team ID #" << firstTeam << endl;
			} else if (strcmp(token, "LASTTEAM") == 0) {
        int lastTeam;
				token = strtok(NULL, " \n");
        lastTeam = strtol(token, NULL, 10);
        numTeams = lastTeam - firstTeam + 1;
        cout << "Number of teams: " << numTeams << endl;
      }
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}

int indexToRobotId(int index) {
  return (index + firstTeam * robotsPerTeam);
}

int robotIdToIndex(int robotId) {
  return (robotId - firstTeam * robotsPerTeam);
}

int indexToTeamId(int index) {
  return (index / robotsPerTeam);
}

int indexToIndexOfTeam(int index) {
  return (index % robotsPerTeam);
}

bool weControlTeam(int teamid) {
  return ((firstTeam <= teamid && teamid < numTeams) ? true : false);
}

int totalOwnRobots() {
  return (numTeams * robotsPerTeam);
}

int teamIdToTeamIndex(int teamid) {
  return (teamid - firstTeam);
}

int teamIndexToTeamId(int teamIndex) {
  return (teamIndex + firstTeam);
}

// Destination function for the AI thread.
void *artificialIntelligence(void *threadid) {
  while (!simulationStarted) {
    // do nothing until simulation starts
    sched_yield();
  }

  ClientRobot clientRobot;
  float velocity = 0.1;
  float angle = 3.0;
  int tempId = 0;
  bool tempFlag = false;
  bool sendNewData = false;

  while (true) {
    if (currentTimestep % 500 == 0) {
      tempFlag = true;
    }
    for (int i = 0; i < totalOwnRobots(); i++) {
      sendNewData = false;
      if (!ownRobots[i]->pendingCommand && currentTimestep % 50 == 0) {
        // Simple AI, follow the leader!
        if (indexToIndexOfTeam(i) == 0 && tempFlag) {
          ownRobots[i]->pendingCommand = true;
          clientRobot.set_id(indexToRobotId(i));
          clientRobot.set_velocityx(((rand() % 11) / 10.0) - 0.5);
          clientRobot.set_velocityy(((rand() % 11) / 10.0) - 0.5);
          clientRobot.set_angle(angle);
          tempId = i;
        } else {
          ownRobots[i]->pendingCommand = true;
          clientRobot.set_id(indexToRobotId(i));
          if (ownRobots[tempId]->x > ownRobots[i]->x) {
            // Move right!
            clientRobot.set_velocityx(velocity);
          } else if (ownRobots[tempId]->x == ownRobots[i]->x) {
            clientRobot.set_velocityx(0.0);
          } else {
            clientRobot.set_velocityx(velocity * -1.0);
          }
          if (ownRobots[tempId]->y > ownRobots[i]->y) {
            // Move up!
            clientRobot.set_velocityy(velocity);
          } else if (ownRobots[tempId]->y == ownRobots[i]->y) {
            clientRobot.set_velocityy(0.0);
          } else {
            clientRobot.set_velocityy(velocity * -1.0);
          }
          clientRobot.set_angle(angle);
        }

        //cout << "vels" << clientRobot.velocityx() << clientRobot.velocityy() << endl;
        theController->queue.push(MSG_CLIENTROBOT, clientRobot);
        theController->set_writing(true);
      } else {
        sched_yield(); // Let the other thread read and write
        //cout << "Pending command exists for robot #" << indexToRobotId(i)
        //     << endl;
      } 
    }

    //sched_yield(); // Let the other thread read and write
    //sleep(5); // delay this thread for 5 seconds
  }

  pthread_exit(0);
}

void run() {
  int controllerfd = -1;
  int currentController = rand() % controllerips.size();
  while(controllerfd < 0)
  {
    cout << "Attempting to connect to controller " << controllerips.at(currentController) << "..." << flush;
    controllerfd = net::do_connect(controllerips.at(currentController).c_str(), CLIENTS_PORT);
    if(0 > controllerfd) {
      cout << " failed to connect." << endl;
    } else if(0 == controllerfd) {
      cerr << " invalid address: " << controllerfd << endl;
    }
    currentController = rand() % controllerips.size();
  }
  //net::set_blocking(controllerfd, false);

  cout << " connected." << endl;
  
  epoll = epoll_create(1); // defined as a gobal variable
  if (epoll < 0) {
    perror("Failed to create epoll handle");
    close(controllerfd);
    exit(1);
  }
  net::EpollConnection controllerconn(epoll, EPOLLIN, controllerfd, 
                                 net::connection::CONTROLLER);
  theController = &controllerconn; // Allows other thread to access. Fix later.

  WorldInfo worldinfo;
  TimestepUpdate timestep;
  ServerRobot serverrobot;
  ClaimTeam claimteam;

  bool foundFirstTeam = false;
  bool foundLastTeam = false;

  #define MAX_EVENTS 128
  struct epoll_event events[MAX_EVENTS];

  // Create thread: client robot calculations
  // parent thread: continue in while loop, looking for updates
  pthread_t aiThread;


  pthread_create(&aiThread, NULL, artificialIntelligence, NULL);

  try {
    while(true) {
      int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);

      for(size_t i = 0; i < (unsigned)eventcount; i++) {
        net::EpollConnection *c = (net::EpollConnection*)events[i].data.ptr;
        if(events[i].events & EPOLLIN) {
          switch(c->type) {
          case net::connection::CONTROLLER:
            // this should be the only type of messages
            MessageType type;
            int len;
            const void *buffer;
            if(c->reader.doRead(&type, &len, &buffer)) {
              switch(type) {
              case MSG_WORLDINFO:
              {
                // Should be the first message we recieve from the controller
                worldinfo.ParseFromArray(buffer, len);
                int robotSize = worldinfo.robot_size();

                int maxTeamId = 0;
                bool sameTeam = true;

                robotsPerTeam = 0; // global var

                // Count number of robots per team 
                for(int i = 0; i < robotSize && sameTeam; i++) {
                  if (worldinfo.robot(i).team() == 0) {
                    robotsPerTeam++;
                  } else {
                    sameTeam = false; 
                  }
                }
                maxTeamId = worldinfo.robot(robotSize - 1).team();

                // Check if the number of teams from WorldInfo is less than
                // the number of teams we want to control per the config file.
                if (numTeams > maxTeamId - firstTeam + 1) {
                  numTeams = maxTeamId - firstTeam + 1;
                }

                cout << "Got worldinfo! Calculated " << numTeams 
                     << " possible controlled teams with " << robotsPerTeam 
                     << " robots each.\n";

                // Claim our teams
                for (int i = 0; i < numTeams; i++) {
                  claimteam.set_id(i + firstTeam);
                  c->queue.push(MSG_CLAIMTEAM, claimteam);
                  c->set_writing(true);
                  cout << "Generating ClaimTeam message for team ID #"
                       << i + firstTeam << endl;
                }

                break;
              }
              case MSG_CLAIMTEAM:
                claimteam.ParseFromArray(buffer, len);
                // Case 1: Granted
                //   Case 1.1: This is the first granted team. Update 
                //   firstTeam and foundFirstTeam.
                //   Case 1.2: This is the nth sequential granted team. 
                //   Ignore. 
                //     Case 1.2.1: This is the last ClaimTeam message. 
                //     Set foundLastTeam and proceed to create OwnRobots.
                //   Case 1.3: Non-granted teams are sandwiched by granted
                //   teams. Ignore.
                // Case 2: Not granted
                //   Case 2.1: No granted teams yet. Ignore.
                //     Case 2.1.1: This was the last ClaimTeam message. Exit.
                //   Case 2.2: Appears after a granted team. Set foundLastTeam
                //   and numTeams. 
                //   Case 2.3: Appears after granted and non-granted teams.
                //   Ignore.
                if (claimteam.granted()) {
                  if (!foundFirstTeam) {
                    firstTeam = claimteam.id();
                    foundFirstTeam = true;
                    cout << "ClaimTeam: Found first team, ID #" << firstTeam 
                         << endl;
                  } else { 
                    // Is this the last ClaimTeam message?
                    if ((int)claimteam.id() == numTeams + firstTeam -1) {
                      foundLastTeam = true;
                    }
                  }
                } else { // claimteam.granted() == false
                  if (!foundFirstTeam) {
                    // Is this the last ClaimTeam message?
                    if ((int)claimteam.id() == numTeams + firstTeam -1) {
                      cout << "Client controls no teams!\n";
                    }
                  } else if (foundFirstTeam && !foundLastTeam) {
                    foundLastTeam = true; 
                  } 
                  // ignore otherwise 
                }

                // Assign teams--can only happen once.
                if (foundLastTeam && !simulationStarted) {
                  numTeams = claimteam.id() - firstTeam + 1;
                  cout << "ClaimTeam: Found last team, ID #" 
                       << claimteam.id() << endl;

                  ownRobots = new OwnRobot*[totalOwnRobots()];
                  for (int i = 0; i < totalOwnRobots(); i++) {
                    // We don't have any initial robot data, yet.
                    ownRobots[i] = new OwnRobot();
                  }

                  enemyRobots = new vector<EnemyRobot*>[numTeams];
                  // Allow AI thread to commence.
                  simulationStarted = true;
                }

                break;
              case MSG_TIMESTEPUPDATE:
                timestep.ParseFromArray(buffer, len);
                currentTimestep = timestep.timestep();

                if (simulationStarted) {
                  // Update all current positions.
                  for (int i = 0; i < totalOwnRobots(); i++) {
                    // TODO: wrapping
                    ownRobots[i]->x += ownRobots[i]->vx;
                    ownRobots[i]->y += ownRobots[i]->vy;
                  }
                  // TODO: Update enemy robots
                }

                break;
              case MSG_SERVERROBOT:
              {
                serverrobot.ParseFromArray(buffer, len);
                if (simulationStarted) {
                  int index = serverrobot.id();
                  int team = indexToTeamId(index);
                  if (weControlTeam(team)) {
                    ownRobots[index]->pendingCommand = false;
                    if (serverrobot.has_velocityx()) 
                      ownRobots[index]->vx = serverrobot.velocityx();
                    if (serverrobot.has_velocityy()) 
                      ownRobots[index]->vy = serverrobot.velocityy();
                    if (serverrobot.has_angle()) 
                      ownRobots[index]->angle = serverrobot.angle();
                    if (serverrobot.has_haspuck()) 
                      ownRobots[index]->hasPuck = serverrobot.haspuck();
                    if (serverrobot.has_x()) 
                      ownRobots[index]->x = serverrobot.x();
                    if (serverrobot.has_y()) 
                      ownRobots[index]->y = serverrobot.y();
                    if (serverrobot.has_hascollided()) 
                      ownRobots[index]->hasCollided = serverrobot.hascollided();

                    // TODO: Add to all other enemyRobot lists
                  }
                }
                break;
              }
              default:
                cout << "Unknown message!" << endl;
                break;
              }
            }
            break;
          default:
            cerr << "Internal error: Got unexpected readable event of type " << c->type << endl;
            break;
          }
        } else if(events[i].events & EPOLLOUT) {
          switch(c->type) {
          case net::connection::CONTROLLER:
            if(c->queue.doWrite()) {
              c->set_writing(false);
            }
            break;
          default:
            cerr << "Internal error: Got unexpected readable event of type " << c->type << endl;
            break;
          }
        }
      }
    }
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }

  // Clean up
  shutdown(controllerfd, SHUT_RDWR);
  close(controllerfd);

  for (int i = 0; i < numTeams * robotsPerTeam; i++) {
    delete ownRobots[i];
  }
  delete[] ownRobots;
  
}

//this is the main loop for the client
int main(int argc, char* argv[])
{
  srand(time(NULL));
	//Print a starting message
	printf("--== Client Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Client Initializing ...\n");
	
	helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	cout<<"Using config file: "<<configFileName<<endl;
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Client Running!\n");
	
	run();
	
	printf("Client Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
