#include "client.h"

/////////////////Variables and Declarations/////////////////
const char *configFileName;
//Game world variables
// TODO: organize/move variables out of client.cpp 
bool simulationStarted = false;
bool simulationEnded = false;
int currentTimestep = 0;
int lastTimestep = 0;
int myTeam;
int robotsPerTeam; 
net::EpollConnection* theController;
int epoll;

// World variables
float sightDistance = 100.0; // TODO: get this from worldinfo packet?

// Stat variables
time_t lastSecond = time(NULL);
int sentMessages = 0;
int receivedMessages = 0;
int pendingMessages = 0;
int timeoutMessages = 0;
int moveUp = 0;
int moveDown = 0;
int moveRight = 0;
int moveLeft = 0;
int moveRandom = 0;

const int COOLDOWN = 10;

OwnRobot** ownRobots;

//Config variables
vector<string> controllerips; //controller IPs 

//Variables for the client viewer only
//No mutex needed as only the client viewer may write and the client will only read
int currentRobot=-1;
ClientViewer* viewer;

////////////////////////////////////////////////////////////

int clientViewerThread( gpointer *ptr )
{
	passToThread *pass = (passToThread*)ptr;
	g_async_queue_ref(pass->asyncQueue);
	viewer = new ClientViewer(pass->argc, pass->argv, pass->asyncQueue, &currentRobot);

	viewer->initClientViewer(pass->numberOfRobots);

	return 0;
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
			if(strcmp(token, "CONTROLLERIP") == 0){
				token = strtok(NULL, " \n");
				string newcontrollerip = token;
				controllerips.push_back(newcontrollerip);
				printf("Storing controller IP: %s\n", newcontrollerip.c_str());
      }
		}
		
		fclose (fileHandle);
	}else
	{
		string error="Error: Cannot open config file: "+(string)configFileName;
		throw SystemError(error);
	}
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

float relDistance(float x1, float y1) {
  return (sqrt(x1*x1 + y1*y1));
}

// Check if the two coordinates are the same, compensating for
// floating-point errors.
bool sameCoordinates(float x1, float y1, float x2, float y2) {
  // From testing, it looks like floating point errors either add or subtract
  // 0.0001.
  float maxError = 0.001;
  if (abs(x1 - x2) > maxError) {
    return false;
  }
  if (abs(y1 - y2) > maxError) {
    return false;
  }
  return true; 
}

void forceSend() {
  while(theController->queue.remaining() > 0)
    theController->queue.doWrite();
}

void executeAi(OwnRobot* ownRobot, int index) {
  // TODO give events to AI handler
  int type = 1; // TEMP, remove after we create AI handler

  ClientRobot clientRobot;
  float velocity = 0.1;
  clientRobot.set_velocityx(ownRobot->vx);
  clientRobot.set_velocityy(ownRobot->vy);
  if (type == 0) {
    // Aggressive robot!
    if (ownRobot->seenRobots.size() > 0) {
      // Find the closest seen robot. If none, do nothing.
      vector<SeenRobot*>::iterator closest;
      float minDistance = 9000.01; // Over nine thousand!
      bool foundRobot = false;
      for (vector<SeenRobot*>::iterator it
          = ownRobot->seenRobots.begin();
          it != ownRobot->seenRobots.end(); it++) {
        if (relDistance((*it)->relx, (*it)->rely) < minDistance &&
            !weControlRobot((*it)->id)) {
          closest = it;
          foundRobot = true;
        }
      }

      if (foundRobot) {
        // TODO: Do trig here?
        if ((*closest)->relx > 0.0 && ownRobot->vx != velocity) {
          // Move right!
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityx(velocity);
        } else if ((*closest)->relx < 0.0 && 
            ownRobot->vx != velocity * -1.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityx(velocity * -1.0);
        } else if ((*closest)->relx == 0.0 && ownRobot->vx != 0.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityx(0.0);
        }
        if ((*closest)->rely > 0.0 && ownRobot->vy != velocity) {
          // Move down!
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityy(velocity);
        } else if ((*closest)->rely < 0.0 && 
            ownRobot->vy != velocity * -1.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityy(velocity * -1.0);
        } else if ((*closest)->rely == 0.0 && ownRobot->vy != 0.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityy(0.0);
        }
      } 
    } else {
      // No seen robots. Make sure we're moving.
      if (ownRobot->vx == 0.0 || ownRobot->vy == 0.0) {
        ownRobot->pendingCommand = true;
        clientRobot.set_velocityx(((rand() % 11) / 10.0) - 0.5);
        clientRobot.set_velocityy(((rand() % 11) / 10.0) - 0.5);
      }
    }
  } else if (type == 1) {
    // Scared robot!
    if (ownRobot->seenRobots.size() > 0) {
      // Find the closest seen robot. If none, do nothing.
      vector<SeenRobot*>::iterator closest;
      float minDistance = 9000.01; // Over nine thousand!
      bool foundRobot = false;
      for (vector<SeenRobot*>::iterator it
          = ownRobot->seenRobots.begin();
          it != ownRobot->seenRobots.end(); it++) {
        if (relDistance((*it)->relx, (*it)->rely) < minDistance) {
          closest = it;
          foundRobot = true;
        }
      }

      if (foundRobot) {
        if ((*closest)->relx <= 0.0 && ownRobot->vx != velocity) {
          // Move right!
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityx(velocity);
          moveRight++;
        } else if ((*closest)->relx > 0.0 && 
            ownRobot->vx != velocity * -1.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityx(velocity * -1.0);
          moveLeft++;
        }
        if ((*closest)->rely <= 0.0 && ownRobot->vy != velocity) {
          // Move down!
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityy(velocity);
          moveDown++;
        } else if ((*closest)->rely > 0.0 && 
            ownRobot->vy != velocity * -1.0) {
          ownRobot->pendingCommand = true;
          clientRobot.set_velocityy(velocity * -1.0);
          moveUp++;
        }
      } 
    } else {
      // No seen robots. Make sure we're moving.
      if (ownRobot->vx == 0.0 || ownRobot->vy == 0.0 
          || currentTimestep - ownRobot->whenLastSent > 10000) {
        ownRobot->pendingCommand = true;
        clientRobot.set_velocityx(((rand() % 11) / 10.0) - 0.5);
        clientRobot.set_velocityy(((rand() % 11) / 10.0) - 0.5);
        moveRandom++;
      }
    }
  }

  // Did we want to send a command?
  if (ownRobot->pendingCommand) { 
    clientRobot.set_id(indexToRobotId(index));
    clientRobot.set_angle(0.0); // TODO: implement angle
    theController->queue.push(MSG_CLIENTROBOT, clientRobot);
    theController->set_writing(true);
    ownRobot->whenLastSent = currentTimestep;
    sentMessages++;
  }
  forceSend();
}


void initializeRobots() {
  ClientRobot clientRobot;

  // Initialize robots to some random velocity.
  for (int i = 0; i < robotsPerTeam; i++) {
    clientRobot.set_id(indexToRobotId(i));
    clientRobot.set_velocityx(((rand() % 11) / 10.0) - 0.5);
    clientRobot.set_velocityy(((rand() % 11) / 10.0) - 0.5);
    clientRobot.set_angle(0.0);
    theController->queue.push(MSG_CLIENTROBOT, clientRobot);
    theController->set_writing(true);
    ownRobots[i]->whenLastSent = currentTimestep;
    sentMessages++;
  }
  forceSend();
}


void run(int argc, char** argv, bool runClientViewer) {
  int controllerfd = -1;
  int currentController = rand() % controllerips.size();
  //initialized in the claim robot message
  GAsyncQueue *asyncQueue=NULL;

  while(controllerfd < 0)
  {
    cout << "Attempting to connect to controller " << controllerips.at(currentController) << "..." << flush;
    controllerfd = net::do_connect(controllerips.at(currentController).c_str(), CLIENTS_PORT);
    if(0 > controllerfd) {
    	throw SystemError("Failed to connect to controller");
    } else if(0 == controllerfd) {
    	throw runtime_error("Invalid controller address");
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

  try {
    while(true) {
      // Stats: Received messages per second
      if (lastSecond < time(NULL)) {
        cout << "Sent " << sentMessages << " per second." << endl;
        cout << "Pending " << pendingMessages << " per second." << endl;
        cout << "Received " << receivedMessages << " per second." << endl;
        cout << "Timeout " << timeoutMessages << " per second." << endl;
        cout << "ControllerQueue " << theController->queue.remaining() << endl;
        cout << "Move up " << moveUp << " per second." << endl;
        cout << "Move down " << moveDown << " per second." << endl;
        cout << "Move right " << moveRight << " per second." << endl;
        cout << "Move left " << moveLeft << " per second." << endl;
        cout << "Move random " << moveRandom << " per second." << endl << endl;
        lastSecond = time(NULL);
        sentMessages = 0;
        pendingMessages = 0;
        receivedMessages = 0;
        timeoutMessages = 0;
        moveUp = 0;
        moveDown = 0;
        moveRight = 0;
        moveLeft = 0;
        moveRandom = 0;
      }

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

                    initializeRobots();

                    //TODO: work in progress. Don't bother testing it.
                    //start a new client viewer thread
                    if(runClientViewer)
                    {
                    	if( !g_thread_supported() )
                    	{
                    		GThread *thread;
                    		GError *error = NULL;

                    		g_thread_init(NULL);
                    		asyncQueue=g_async_queue_new();
                    		passToThread pass(argc, argv, robotsPerTeam, asyncQueue );
                    		thread = g_thread_create((GThreadFunc)clientViewerThread, (gpointer)&pass, FALSE, &error);

                    		if( thread == NULL)
                    		{
                    			string tmp="Failed to created a thread: " + (string)error->message;

                    			throw SystemError(tmp);
                    		}
                    	}
                    	else
                    	{
                    		throw SystemError("gthreads are not supported.");
                    	}
                    }

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
                    if (ownRobots[i]->pendingCommand && 
                        currentTimestep - ownRobots[i]->whenLastSent > 100) {
                      // If we've waited too long for an update, send
                      // new ClientRobot messages.
                      // TODO: Lower from 100.
                      ownRobots[i]->pendingCommand = false;
                      timeoutMessages++;
                    }
                     
                    // Update rel distance of seenRobots.
                    float minDistance = 9000.01;
                    float tempDistance;
                    int newClosestRobotId = -1;
                    for (vector<SeenRobot*>::iterator it
                        = ownRobots[i]->seenRobots.begin();
                        it != ownRobots[i]->seenRobots.end();
                        it++) {
                      (*it)->relx += (*it)->vx - ownRobots[i]->vx;
                      (*it)->rely += (*it)->vy - ownRobots[i]->vy;
                      tempDistance = relDistance((*it)->relx, (*it)->rely);
                      if (tempDistance > sightDistance) {
                        // We can't see the robot anymore. Delete.
                        delete *it; 
                        it = ownRobots[i]->seenRobots.erase(it);
                        it--; // Compensates for it++ in for loop.
                      }
                      else if (tempDistance < minDistance) {
                        // Keep trying to find the closest robot!
                        minDistance = tempDistance;
                        newClosestRobotId = (*it)->id;
                      }
                    }

                    // Update rel distance of seenPucks.
                    for (vector<SeenPuck*>::iterator it
                        = ownRobots[i]->seenPucks.begin();
                        it != ownRobots[i]->seenPucks.end();
                        it++) {
                      (*it)->relx -= ownRobots[i]->vx;
                      (*it)->rely -= ownRobots[i]->vy;
                      tempDistance = relDistance((*it)->relx, (*it)->rely);
                      //cout << "#" << i << " puckRelX = " << (*it)->relx
                      //     << ", puckRelY = " << (*it)->rely << endl;
                      if (tempDistance > sightDistance) {
                        // We can't see the puck anymore. Delete.
                        delete *it; 
                        it = ownRobots[i]->seenPucks.erase(it);
                        it--; // Compensates for it++ in for loop.
                      }
                    }

                    // Check for events
                    if (newClosestRobotId != ownRobots[i]->closestRobotId) {
                      ownRobots[i]->closestRobotId = newClosestRobotId;
                      ownRobots[i]->eventQueue.push_back(EVENT_NEW_CLOSEST_ROBOT);
                      // Check if any events exist; if so, call AI.
                      if (ownRobots[i]->eventQueue.size() > 0 &&
                          !ownRobots[i]->pendingCommand) {
                        executeAi(ownRobots[i], i); 
                        // Clear the queue, wait for new events.
                        ownRobots[i]->eventQueue.clear();
                      }
                    } 
                  }
                }
                //TODO: Egor - client viewer work in progress
                if(runClientViewer)
                {
                	g_async_queue_push( asyncQueue, (gpointer) ownRobots[currentRobot] );
                	viewer->updateViewer();
                }

                break;
              case MSG_SERVERROBOT:
              { 
                serverrobot.ParseFromArray(buffer, len);
                receivedMessages++;
                if (simulationStarted) {
                  int robotId = serverrobot.id();
                  if (weControlRobot(robotId)) {
                    // The serverrobot is from our team.
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
                    if (serverrobot.has_hascollided()) 
                      ownRobots[index]->hasCollided = serverrobot.hascollided();
                  }

                  // Traverse seenById list to check if we can see.
                  int index;
                  int listSize = serverrobot.seesserverrobot_size();
                  for (int i = 0; i < listSize; i++) {
                    if (weControlRobot(serverrobot.seesserverrobot(i).seenbyid())) {
                      // The serverrobot is not on our team. Can we see it?
                      index = robotIdToIndex(serverrobot.seesserverrobot(i).
                          seenbyid());
                      if (serverrobot.seesserverrobot(i).viewlostid()) {
                        // Could see before, can't see anymore.
                        for (vector<SeenRobot*>::iterator it
                            = ownRobots[index]->seenRobots.begin();
                            it != ownRobots[index]->seenRobots.end();
                            it++) {
                          if ((*it)->id == serverrobot.id()) {
                            // TODO: we may want to store data about this
                            // robot on the client for x timesteps after
                            // it can't see it anymore.
                            //cout << "Our #" << index << " lost see "
                            //     << serverrobot.id() << endl;
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
                          if ((*it)->id == serverrobot.id()) {
                            foundRobot = true;
                            //cout << "Our #" << index << " update see "
                            //     << serverrobot.id() << " at relx: " 
                            //     << serverrobot.seesserverrobot(i).relx() << endl;
                            bool stateChange = false;
                            if (serverrobot.has_velocityx() && 
                                (*it)->vx != serverrobot.velocityx()) {
                              (*it)->vx = serverrobot.velocityx();
                              stateChange = true;
                            }
                            if (serverrobot.has_velocityy() &&
                                (*it)->vy != serverrobot.velocityy()) {
                              (*it)->vy = serverrobot.velocityy();
                              stateChange = true;
                            }
                            if (serverrobot.has_angle() &&
                                (*it)->angle != serverrobot.angle()) {
                              (*it)->angle = serverrobot.angle();
                              stateChange = true;
                            }
                            if (serverrobot.has_haspuck() &&
                                (*it)->hasPuck != serverrobot.haspuck()) {
                              (*it)->hasPuck = serverrobot.haspuck();
                              stateChange = true;
                            }
                            if (serverrobot.has_hascollided() && 
                                (*it)->hasCollided != serverrobot.hascollided()) {
                              (*it)->hasCollided = serverrobot.hascollided();
                              stateChange = true;
                            }
                            if (serverrobot.seesserverrobot(i).has_relx())
                              (*it)->relx = serverrobot.seesserverrobot(i).relx();
                            if (serverrobot.seesserverrobot(i).has_rely())
                              (*it)->rely = serverrobot.seesserverrobot(i).rely();
                            (*it)->lastTimestepSeen = currentTimestep;

                            // If we updated the closest robot, tell the AI.
                            if (stateChange && serverrobot.id() == 
                                ownRobots[index]->closestRobotId) {
                              ownRobots[index]->eventQueue.push_back(
                                  EVENT_CLOSEST_ROBOT_STATE_CHANGE);
                            } 
                          }
                        }
                        if (!foundRobot) {
                          SeenRobot *r = new SeenRobot();
                          //cout << "Our #" << index << " begin see "
                          //     << serverrobot.id() << endl;
                          if (serverrobot.has_velocityx()) 
                            r->vx = serverrobot.velocityx();
                          if (serverrobot.has_velocityy()) 
                            r->vy = serverrobot.velocityy();
                          if (serverrobot.has_angle()) 
                            r->angle = serverrobot.angle();
                          if (serverrobot.has_haspuck()) 
                            r->hasPuck = serverrobot.haspuck();
                          if (serverrobot.has_hascollided()) 
                            r->hasCollided = serverrobot.hascollided();
                          r->id = serverrobot.id();
                          r->relx = serverrobot.seesserverrobot(i).relx();
                          r->rely = serverrobot.seesserverrobot(i).rely();
                          r->lastTimestepSeen = currentTimestep;
                          ownRobots[index]->seenRobots.push_back(r);
                        }
                      }
                    }
                  }
                }
                break;
              }
              case MSG_PUCKSTACK:
              { 
                PuckStack puckstack;
                puckstack.ParseFromArray(buffer, len);
                if (simulationStarted) {
                  int index;
                  int listSize = puckstack.seespuckstack_size();
                  for (int i = 0; i < listSize; i++) {
                    // Check if one of our robots is in the list. If it is,
                    // add the puck to its seenPuck list. Pretend that every
                    // puck is a new, previously-unseen puck.
                    if (weControlRobot(puckstack.seespuckstack(i).seenbyid())) {
                      index = robotIdToIndex(puckstack.seespuckstack(i).
                          seenbyid()); // Our robot that can see.
                      int oldSeenPuckSize = ownRobots[index]->seenPucks.size();

                      // Look through seenPuck list, check if the new puck's
                      // position is the same as one we already see. If so,
                      // update our stacksize. 
                      bool foundPuck = false;
                      for (vector<SeenPuck*>::iterator it
                          = ownRobots[index]->seenPucks.begin();
                          it != ownRobots[index]->seenPucks.end() 
                          && !foundPuck; it++) {
                        if (sameCoordinates((*it)->relx, (*it)->rely, 
                            puckstack.seespuckstack(i).relx(),
                            puckstack.seespuckstack(i).rely())) {
                          (*it)->stackSize = puckstack.stacksize();
                          foundPuck = true;
                          cout << "Updating puck stacksize!" << endl;
                          if ((*it)->stackSize <= 0) {
                            cout << "But stacksize is 0, so delete!" << endl;
                            delete *it; 
                            it = ownRobots[index]->seenPucks.erase(it);
                          }
                        }
                      }

                      // If we didn't find a match in seenPucks, add new puck.
                      if (!foundPuck) {
                        SeenPuck *p = new SeenPuck();
                        p->stackSize = puckstack.stacksize();
                        p->relx = puckstack.seespuckstack(i).relx();
                        p->rely = puckstack.seespuckstack(i).rely();
                        ownRobots[index]->seenPucks.push_back(p);
                      }

                      int newSeenPuckSize = ownRobots[index]->seenPucks.size();
                      // Check for seenPuck size changes to create events.
                      if (oldSeenPuckSize == 0 && newSeenPuckSize > 0) {
                        ownRobots[index]->eventQueue.push_back(
                            EVENT_START_SEEING_PUCKS);
                      } else if (oldSeenPuckSize > 0 && newSeenPuckSize == 0) {
                        ownRobots[index]->eventQueue.push_back(
                            EVENT_END_SEEING_PUCKS);
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

  // Clean up
  shutdown(controllerfd, SHUT_RDWR);
  close(controllerfd);

  // Free memory allocated for ownRobots. Only do this if we actually
  // started the simulation and allocated space.
  if (simulationStarted) {
    for (int i = 0; i < robotsPerTeam; i++) {
      delete ownRobots[i];
    }
    delete[] ownRobots;
  }
}
void tmpTestViewer(int argc, char** argv, bool runClientViewer)
{
	if(runClientViewer)
	{
		//make VERY sure that the client viewer thread gets created once
		runClientViewer=false;
		if( !g_thread_supported() )
		{
    		GThread *thread;
    		GError *error = NULL;

    		g_thread_init(NULL);
			GAsyncQueue* asyncQueue=g_async_queue_new();
    		passToThread pass(argc, argv, 1000, asyncQueue );
			thread = g_thread_create((GThreadFunc)clientViewerThread, (gpointer)&pass, TRUE, &error);
			g_thread_join(thread);
		}
	}
}

//this is the main loop for the client
int main(int argc, char* argv[])
{
	bool runClientViewer = false;
	srand(time(NULL));
	//Print a starting message
	printf("--== Client Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Client Initializing ...\n");
	
	helper::CmdLine cmdline(argc, argv);

	configFileName=cmdline.getArg("-c", "config").c_str();
	cout<<"Using config file: "<<configFileName<<endl;
	loadConfigFile();

	runClientViewer=cmdline.getArg("-viewer").length() ? true : false;
	cout<<"Started client with the client viewer set to "<<runClientViewer<<endl;
	//tmpTestViewer(argc, argv, runClientViewer);

	myTeam = strtol(cmdline.getArg("-t", "0").c_str(), NULL, 0);
	cout << "Trying to control team #" << myTeam << " (use -t <team> to change)"
       << endl;
	////////////////////////////////////////////////////
	
	printf("Client Running!\n");
	run(argc, argv, runClientViewer);
	
	printf("Client Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
