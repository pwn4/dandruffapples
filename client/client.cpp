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
#include "../common/puckstack.pb.h"
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
using namespace google;
using namespace protobuf;

/////////////////Variables and Declarations/////////////////
const char *configFileName;

//Game world variables
// TODO: organize/move variables out of client.cpp 
bool simulationStarted = false;
bool simulationEnded = false;
int currentTimestep = 0;
int myTeam;
int robotsPerTeam; 
net::EpollConnection* theController;
int epoll;

class SeenPuck {
  float relX;
  float relY;
  int size;

  SeenPuck() : relX(0.0), relY(0.0), size(1) {}
};

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

class SeenRobot : public Robot {
public:
  int id;
  int lastTimestepSeen;
  bool viewable;
  float relX;
  float relY;

  SeenRobot() : Robot(), id(-1), lastTimestepSeen(-1), viewable(true),
      relX(0.0), relY(0.0) {}
};

class OwnRobot : public Robot {
public:
  bool pendingCommand;
  vector<SeenRobot*> seenRobots;
  vector<SeenPuck*> seenPucks;

  OwnRobot() : Robot(), pendingCommand(false) {}
};

OwnRobot** ownRobots;

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
      }
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}

int indexToRobotId(int index) {
  return (index + myTeam * robotsPerTeam);
}

int robotIdToIndex(int robotId) {
  return (robotId - myTeam * robotsPerTeam);
}

bool weControlRobot(int robotId) {
  int index = robotId - myTeam * robotsPerTeam;
  return (0 <= index && index < robotsPerTeam);
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

  while (!simulationEnded) {
    if (currentTimestep % 500 == 0) {
      tempFlag = true;
    }
    for (int i = 0; i < robotsPerTeam && !simulationEnded; i++) {
      sendNewData = false;
      if (!ownRobots[i]->pendingCommand && currentTimestep % 50 == 0) {
        // Simple AI, follow the leader!
        if (i == 0 && tempFlag) {
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

  simulationStarted = false;
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
  net::set_blocking(controllerfd, false);

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
                cout << "Got worldinfo! Calculated " << robotsPerTeam 
                     << " robots on each team.\n";

                // Claim our team
                claimteam.set_id(myTeam);
                c->queue.push(MSG_CLAIMTEAM, claimteam);
                c->set_writing(true);
                cout << "Generating ClaimTeam message for team ID #"
                     << myTeam << endl;

                break;
              }
              case MSG_CLAIMTEAM:
                claimteam.ParseFromArray(buffer, len);
                if (!simulationStarted) {
                  if (claimteam.granted()) {
                    myTeam = claimteam.id();
                    cout << "ClaimTeam: Success! We control team #" << myTeam 
                           << endl;
                  } else { // claimteam.granted() == false
                    myTeam = -1;
                    cout << "Client controls no teams!\n";
                  }

                  // Assign teams--can only happen once.
                  if (myTeam > -1) {
                    ownRobots = new OwnRobot*[robotsPerTeam];
                    for (int i = 0; i < robotsPerTeam; i++) {
                      // We don't have any initial robot data, yet.
                      ownRobots[i] = new OwnRobot();
                    }

                    //enemyRobots = new vector<EnemyRobot*>[numTeams];
                    // Allow AI thread to commence.
                    simulationStarted = true;
                  }
                } else {
                  cout << "Got CLAIMTEAM message after simulation started"
                       << endl;
                }

                break;
              case MSG_TIMESTEPUPDATE:
                timestep.ParseFromArray(buffer, len);
                currentTimestep = timestep.timestep();

                if (simulationStarted) {
                  // Update all current positions.
                  for (int i = 0; i < robotsPerTeam; i++) {
                    // TODO: Change to relative positions
                    ownRobots[i]->x += ownRobots[i]->vx;
                    ownRobots[i]->y += ownRobots[i]->vy;
                    for (vector<SeenRobot*>::iterator it
                        = ownRobots[i]->seenRobots.begin();
                        it != ownRobots[i]->seenRobots.end();
                        it++) {
                      // TODO: Update enemy robots in ownrobot lists
                    }
                  }
                }

                break;
              case MSG_SERVERROBOT:
              {
                serverrobot.ParseFromArray(buffer, len);
                if (simulationStarted) {
                  int robotId = serverrobot.id();
                  if (weControlRobot(robotId)) {
                    int index = robotIdToIndex(robotId);
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
                  }

                  // Traverse seenById list to check if we can see.
                  int index;
                  int listSize = serverrobot.seenrobot_size();
                  for (int i = 0; i < listSize; i++) {
                    if (weControlRobot(serverrobot.seenrobot(i).seenbyid())) {
                      index = robotIdToIndex(serverrobot.seenrobot(i).
                          seenbyid());
                      if (serverrobot.seenrobot(i).viewlostid()) {
                        // Could see before, can't see anymore.
                        for (vector<SeenRobot*>::iterator it
                            = ownRobots[index]->seenRobots.begin();
                            it != ownRobots[index]->seenRobots.end();
                            it++) {
                          if ((*it)->id == serverrobot.seenrobot(i).seenbyid()) {
                            // TODO: we may want to store data about this
                            // robot on the client for x timesteps after
                            // it can't see it anymore.
                            cout << "Our #" << index << " lost see "
                                 << serverrobot.id() << endl;
                            delete *it; 
                            ownRobots[index]->seenRobots.erase(it);
                            break;
                          }
                        }
                      } else {
                        // Can see. Add, or update?
                        bool foundRobot = false;
                        for (vector<SeenRobot*>::iterator it
                            = ownRobots[index]->seenRobots.begin();
                            it != ownRobots[index]->seenRobots.end() &&
                            !foundRobot; it++) {
                          if ((*it)->id == serverrobot.seenrobot(i).seenbyid()) {
                            foundRobot = true;
                            cout << "Our #" << index << " update see "
                                 << serverrobot.id() << " at relx: " 
                                 << serverrobot.seenrobot(i).relx() << endl;
                            // TODO: Update fields SeenRobot!
                          }
                        }
                        if (!foundRobot) {
                          SeenRobot *r = new SeenRobot();
                          r->id = serverrobot.seenrobot(i).seenbyid();
                          // TODO: Update seen robot fields.
                          cout << "Our #" << index << " begin see "
                               << serverrobot.id() << endl;
                          ownRobots[index]->seenRobots.push_back(r);
                        }
                      }
                    }
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

  simulationEnded = true;
  while (simulationStarted) {
   // Wait until child thread stops
   sched_yield();
  } 

  // Clean up
  shutdown(controllerfd, SHUT_RDWR);
  close(controllerfd);

  for (int i = 0; i < robotsPerTeam; i++) {
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

  myTeam = (config.getArg("-t").length() == 0 ? 0 
      : strtol(config.getArg("-t").c_str(), NULL, 0));

  cout << "Trying to control team #" << myTeam << " (use -t <team> to change)"
       << endl;
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Client Running!\n");
	
	run();
	
	printf("Client Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
