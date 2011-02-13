/*/////////////////////////////////////////////////////////////////////////////////////////////////
Client program
This program communications with controllers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/
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
#include <sys/epoll.h>

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "../common/ports.h"
#include "../common/clientrobot.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/timestep.pb.h"
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
int currentTimestep = 0;
int firstRobot; // offset, lets us control robots 600-1000, for example
int firstTeam; // lowest teamid we control
int numTeams; // this client computer controls this number of teams.
int numRobots; // number of robots per team
helper::connection* theController;
int epoll;

struct OwnRobot {
  float x;
  float y;
  float velocity;
  float angle;
  bool hasPuck;
  bool hasCollided;  
};

OwnRobot* ownRobots; 


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

void *artificialIntelligence(void *threadid) {
  while (currentTimestep < 1) {
    // do nothing until simulation starts
    sched_yield();
  }

  struct epoll_event event; 
  ClientRobot clientRobot;
  int id = 1337;
  float velocity = 0.1;
  float angle = 3.0;


  // Make robot #1337 do something every 5 seconds 
  while (true) {
    cout << "AI thread creating ClientRobot message at timestep "
         << currentTimestep << endl; 
    clientRobot.set_id(id);
    clientRobot.set_velocity(velocity);
    clientRobot.set_angle(angle);

    theController->queue.push(MSG_CLIENTROBOT, clientRobot);
    event.events = EPOLLIN | EPOLLOUT; 
    event.data.ptr = theController;
    epoll_ctl(epoll, EPOLL_CTL_MOD, theController->fd, &event);

    sched_yield(); // Let the other thread read and write
    sleep(5); // delay this thread for 5 seconds
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
  helper::connection controllerconn(controllerfd, 
                                    helper::connection::CONTROLLER);
  theController = &controllerconn; // Allows other thread to access. Fix later.

  //epoll setup
  struct epoll_event event; 
  event.events = EPOLLIN;
  event.data.ptr = &controllerconn;
  if (0 > epoll_ctl(epoll, EPOLL_CTL_ADD, controllerfd, &event)) {
    perror("Failed to add controller socket to epoll");
    close(controllerfd);
    exit(1);
  }

  TimestepUpdate timestep;
  ClientRobot clientRobot;

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
        helper::connection *c = (helper::connection*)events[i].data.ptr;
        if(events[i].events & EPOLLIN) {
          switch(c->type) {
          case helper::connection::CONTROLLER:
            // this should be the only type of messages
            MessageType type;
            size_t len;
            const void *buffer;
            if(c->reader.doRead(&type, &len, &buffer)) {
              switch(type) {
              case MSG_TIMESTEPUPDATE:
                timestep.ParseFromArray(buffer, len);
                currentTimestep = timestep.timestep();

                if (currentTimestep % 10000 == 0) {
                  cout << "Generating ClientRobot message at timestep " 
                       << currentTimestep << endl;
                  // Test something every 10000 timesteps
                  int id = 0;
                  float velocity = 0.1;
                  float angle = 3.0;
                  clientRobot.set_id(id);
                  clientRobot.set_velocity(velocity);
                  clientRobot.set_angle(angle);

                  c->queue.push(MSG_CLIENTROBOT, clientRobot);
                  event.events = EPOLLIN | EPOLLOUT; // just EPOLLOUT?
                  event.data.ptr = c;
                  epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
                }
                break;
              case MSG_SERVERROBOT:
                cout << "Received ServerRobot update!" << endl;
                // update robot position global variables
                break;
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
          case helper::connection::CONTROLLER:
            if(c->queue.doWrite()) {
              cout << "Sending ClientRobot message at timestep " 
                   << currentTimestep << endl;
              event.events = EPOLLIN;
              event.data.ptr = c;
              epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
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
}

//this is the main loop for the client
int main(int argc, char* argv[])
{
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
