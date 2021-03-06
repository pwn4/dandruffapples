#include "client.h"
#include <dlfcn.h>
#include <sstream>

// Seconds
#define CONNECT_TIMEOUT 480

typedef ClientAi* (*FUNCPTR_AI)();

/////////////////Variables and Declarations/////////////////
//Game world variables
// TODO: organize/move variables out of client.cpp
bool simulationStarted = false;
int Client::currentTimestep = 0;
int lastTimestep = 0;
int robotsPerTeam;


////////////////////////////////////////////////////////////////////////
// Client
////////////////////////////////////////////////////////////////////////

//this function loads the config file so that the server parameters don't need to be added every time
void Client::loadConfigFile(const char* configFileName, string& pathToExe) {
	//open the config file
	FILE * fileHandle;
	fileHandle = fopen(configFileName, "r");

	//create a read buffer. No line should be longer than 200 chars long.
	char readBuffer[200];
	char * token;

	if (fileHandle != NULL) {
		int accumulatedWeight = 0; // for AI loading
		while (fgets(readBuffer, sizeof(readBuffer), fileHandle) != 0) {
			token = strtok(readBuffer, " \n");

			//if it's a REGION WIDTH definition...
			if (strcmp(token, "CONTROLLERIP") == 0) {
				token = strtok(NULL, " \n");
				string newcontrollerip = token;
				controllerips.push_back(newcontrollerip);
				cout << "Storing controller IP: " + newcontrollerip << endl;
			}

			//if it's a Client AI definition, load the shared lib
			if (strcmp(token, "AI") == 0) {
				token = strtok(NULL, " \n");
				string aiLib = token;
				token = strtok(NULL, " \n");
				stringstream ss(token);
				int weight;
				ss >> weight;

				// Load the library
				char* err_msg = NULL;
				void* hndl = dlopen((pathToExe+aiLib).c_str(), RTLD_NOW);
				if (hndl == NULL) {
					cerr << dlerror() << endl;
					exit(-1);
				} else {
					cout << "AI loaded from " << pathToExe+aiLib << "...";
				}
				cout << " weight: " << weight << "." << endl;

				// get AI code
				FUNCPTR_AI maker = (FUNCPTR_AI)dlsym(hndl, "maker");
				err_msg = dlerror();
				if (err_msg) { cout << err_msg << endl; exit(-1); }
				// create our AI object
				ClientAi* ai = (ClientAi*)((*maker)());
				accumulatedWeight += weight;
				clientAiList.push_back(make_pair(ai, accumulatedWeight));

				//TODO: put the cleanup code somewhere
				/*dlclose(hndl);
				err_msg = dlerror();
				if (err_msg) { cout << err_msg << endl; exit(-1); }
				*/
			}
		}

		fclose(fileHandle);
	} else {
		string error = "Error: Cannot open config file: " + (string) configFileName;
		throw SystemError(error);
	}
}

//FROM THE AREA ENGINE
//this method checks if a robot at (x1,y1) sees a robot at (x2,y2)
double sightSquare = (VIEWDISTANCE+ROBOTDIAMETER)*(VIEWDISTANCE+ROBOTDIAMETER);
bool Sees(double x1, double y1){
//assumes robots can see from any part of themselves
  if((x1)*(x1) + (y1)*(y1) <= sightSquare)
    return true;
  return false;
}

bool InViewAngle(float myangle, double x1, double y1)
{
  //get the angle to x1 and y1
  float theirangle = hack_angleFromVelocity(x1, y1);
  
  if(abs(myangle - theirangle) < VIEWANGLE)
    return true;
  
  return false;
}

// Adds a new controller address to the client's known list of controllers.
void Client::setControllerIp(string newControllerIp) {
	controllerips.clear();
	controllerips.push_back(newControllerIp);
}

// Gives the client a teamId. The client controls this team's robots. The
// myTeam variable is used to convert robotIds (1-1000000) to indexes in
// the local ownRobot data structure (0-999).
void Client::setMyTeam(int myTeam) {
	this->myTeam = myTeam;
}

// Input: Index from local ownRobot array.
// Output: The robotId used by the server.
unsigned int Client::indexToRobotId(int index) {
	return (index + 1 + myTeam * robotsPerTeam);
}

// Input: The robotId used by the server.
// Output: Index from local ownRobot array.
int Client::robotIdToIndex(int robotId) {
	return (robotId - 1 - myTeam * robotsPerTeam);
}

// Returns true if the specified robotId is a member of our team.
bool Client::weControlRobot(int robotId) {
	int index = robotIdToIndex(robotId);
	return (0 <= index && index < robotsPerTeam);
}

// Uses Pythagoras to find the distance of some object to its origin. Useful
// for calculating the distance from homes, pucks, and other robots.
double Client::relDistance(double x1, double y1) {
	return (sqrt(x1 * x1 + y1 * y1));
}

// Make sure 0 <= angle < 2PI. If angle is outside this range, either add or
// subtract by 2PI to put back in the range.
double Client::verifyAngle(double angle) {
  double twoPi = 6.283185307; // TODO: Should be a constant.
  while (angle < 0) {
    // Increment angle until it is in proper range.
    angle += twoPi;
  }
  while (angle >= twoPi) {
    // Decrement angle until it is in proper range.
    angle -= twoPi;
  }

  return angle;
}

// If the robot is turning, then a) figure out if we want to be turning
// clockwise or counter-clockwise and b) make sure that the angle is valid
// after we increment/decrement it. 
/*void Client::estimateRotation(OwnRobot* ownRobot) {
  if (ownRobot->desiredAngle != ownRobot->angle) {
    // We are turning!
    double magicNumber = 0.1256637061; // 1/50 * 2PI
    // TODO: Use a world global variable instead of magic numbers!
    double cwRotation = verifyAngle(ownRobot->desiredAngle - ownRobot->angle);
    double ccwRotation = verifyAngle((
        ownRobot->angle - ownRobot->desiredAngle) * -1);
    if (cwRotation < ccwRotation) {
      // Turn clockwise!
      ownRobot->angle = verifyAngle(ownRobot->angle + magicNumber);
      cwRotation = verifyAngle(ownRobot->desiredAngle - ownRobot->angle);
      ccwRotation = verifyAngle((
        ownRobot->angle - ownRobot->desiredAngle) * -1);
      if (cwRotation > ccwRotation 
          || ownRobot->angle == ownRobot->desiredAngle) {
        // Turned too far!
        ownRobot->angle = ownRobot->desiredAngle;
      }
    } else {
      // Turn counter-clockwise!
      ownRobot->angle = verifyAngle(ownRobot->angle - magicNumber);
      cwRotation = verifyAngle(ownRobot->desiredAngle - ownRobot->angle);
      ccwRotation = verifyAngle((
        ownRobot->angle - ownRobot->desiredAngle) * -1);
      if (ccwRotation > cwRotation 
          || ownRobot->angle == ownRobot->desiredAngle) {
        // Turned too far!
        ownRobot->angle = ownRobot->desiredAngle;
      }
    }
  }
}*/

// Check if the two coordinates are the same, compensating for
// doubleing-point errors.
bool Client::sameCoordinates(double x1, double y1, double x2, double y2) {
	// From testing, it looks like doubleing point errors either add or subtract
	// 0.0001.
	double maxError = 0.1; // TODO: Lower the error if possible!
	if (abs(x1 - x2) > maxError) {
		return false;
	}
	if (abs(y1 - y2) > maxError) {
		return false;
	}
	return true;
}

int * state = NULL;
ClientRobotCommand Client::userAiCode(OwnRobot* ownRobot) {
	ClientRobotCommand cmd;
	if (ownRobot->ai) // robots don't have ai at the first timestep?
		ownRobot->ai->make_command(cmd, ownRobot);
	return cmd;
}

// Provides the framework needed to generate an AI command for a robot.
// Handles the ClientRobot message creation. Calls the user's AI code. Sends
// the ClientRobot message over the network if the AI decided to send a
// command. Sets pendingCommand to true for the robot, so that no more
// commands can be executed for this robot until we get a response from
// the server.
void Client::executeAi(OwnRobot* ownRobot, int index, net::connection &controller) {
  // Have we sent a ClientRobot message without receiving a ServerRobot
  // message for our robot? If so, don't send any commands yet.
	if (ownRobot->pendingCommand)
		return;

  // Prepare the ClientRobot message. Initialize clientrobot message
  // to current values.
	ClientRobot clientrobot;
	clientrobot.set_id(indexToRobotId(index));
	clientrobot.set_velocityx(ownRobot->vx);
	clientrobot.set_velocityy(ownRobot->vy);
 	//clientrobot.set_angle(ownRobot->angle);

  // Allow the user AI code to run.
  
  //but let's just be cheapo and only give the AI the robots it can actually see
	vector<SeenRobot*> tmpRobots;
	vector<SeenPuck*> tmpPucks;
	
	//yeah, it's slow. But if its good enough, its good enough.
	vector<SeenRobot*>::iterator robotFilter = ownRobot->seenRobots.begin();
	vector<SeenPuck*>::iterator puckFilter = ownRobot->seenPucks.begin();
	
	//only trim if we're moving. Otherwise, show all
	if(ownRobot->vx != 0 || ownRobot->vy != 0)
	{
	  for(; robotFilter != ownRobot->seenRobots.end(); )
	  {  
	    if(!InViewAngle(ownRobot->angle, (*robotFilter)->relx, (*robotFilter)->rely)){
	      tmpRobots.push_back(*robotFilter);
	      robotFilter = ownRobot->seenRobots.erase(robotFilter);
	      continue;
	    }
	    robotFilter++;
    }
	    
    for(; puckFilter != ownRobot->seenPucks.end();)
	  {
	    if(!InViewAngle(ownRobot->angle, (*puckFilter)->relx, (*puckFilter)->rely)){
	      tmpPucks.push_back(*puckFilter);
	      puckFilter = ownRobot->seenPucks.erase(puckFilter);
	      continue;
	    }		    
	    puckFilter++;
    }
  }
  
	ClientRobotCommand command = userAiCode(ownRobot);
	if (command.sendCommand) {
		if (command.changeVx)
			clientrobot.set_velocityx(command.vx);
		if (command.changeVy)
			clientrobot.set_velocityy(command.vy);
		//angle changes should be done by changing velocity	
		if (false && command.changeAngle) {
      command.angle = verifyAngle(command.angle); // 0 <= angle < 2PI 
			clientrobot.set_angle(command.angle);
      ownRobot->desiredAngle = command.angle; 
    }
		if (command.changePuckPickup) {
			puckPickupMessages++; // debug message
			clientrobot.set_puckpickup(command.puckPickup);
		}

    // Send the ClientRobot message!
		controller.queue.push(MSG_CLIENTROBOT, clientrobot);

		ownRobot->whenLastSent = currentTimestep;
		sentMessages++; // debug message
		ownRobot->pendingCommand = true;   
	}
	
		//then restore those robots
	for(robotFilter = tmpRobots.begin(); robotFilter != tmpRobots.end(); robotFilter++)
    ownRobot->seenRobots.push_back(*robotFilter);
  
  for(puckFilter = tmpPucks.begin(); puckFilter != tmpPucks.end(); puckFilter++)
    ownRobot->seenPucks.push_back(*puckFilter);
}

// Sends an initial ClientRobot message for each robot, allowing us to
// define initial velocities. Assigns robot "behaviours" to our robots
// based on the AI weights defined in client/config.
void Client::initializeRobots(net::connection &controller) {
	ClientRobot clientRobot;
	int max_weight = clientAiList[clientAiList.size()-1].second;

	// Initialize robots to some random velocity.
	for (int i = 0; i < robotsPerTeam; i++) {
		clientRobot.set_id(indexToRobotId(i));
		//clientRobot.set_velocityx(((rand() % 11) / 10.0) - 0.5);
		//clientRobot.set_velocityy(((rand() % 11) / 10.0) - 0.5);
		clientRobot.set_velocityx(0.0);
		clientRobot.set_velocityy(0.0);
		clientRobot.set_angle(0.0);
		controller.queue.push(MSG_CLIENTROBOT, clientRobot);
		ownRobots[i]->whenLastSent = currentTimestep;
		sentMessages++;

		// not used
		// ownRobots[i]->behaviour = i % 2;

		// set AI for this robot according to weights
		int randomWeight = rand() % max_weight;
		for (size_t k = 0; k < clientAiList.size(); k++) {
			if (randomWeight < clientAiList[k].second) {
				ownRobots[i]->ai = clientAiList[k].first;
				break;
			}
		}
	}
}

gboolean runner(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	passToRun *passer = (passToRun*)data;
	return passer->client->run(ioch, cond, data);
}

// The main loop of the client program! Handles all network input.
// Simulates robot updates. Calls the user AI code each timestep.
gboolean Client::run(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	MessageType type;
	int len;
	const void *buffer;
	passToRun *passer = (passToRun*)data;

	bool &runClientViewer=passer->runClientViewer;
	net::connection &controller=passer->controller;
	ClientViewer* &viewer = passer->viewer;

  if(cond & G_IO_OUT) {
    if(controller.queue.doWrite()) {
      // We don't need to check writability for now
      g_source_remove(gwatch);
      gwatch = g_io_add_watch(ioch, G_IO_IN, runner, data);
      writing = false;
    }
  }

#ifdef DEBUG_COUT
	// Stats: Received messages per second
	if (lastSecond < time(NULL)) {
		cout << "Sent " << sentMessages << " per second." << endl;
		//cout << "Pending " << pendingMessages << " per second." << endl;
		cout << "Received " << receivedMessages << " per second." << endl;
		cout << "Timeout " << timeoutMessages << " per second." << endl;
		cout << "ControllerQueue " << controller.queue.remaining() << endl;
		cout << "Puck pickups " << puckPickupMessages << " per second." << endl;
		cout << endl;
		lastSecond = time(NULL);
		sentMessages = 0;
		pendingMessages = 0;
		receivedMessages = 0;
		timeoutMessages = 0;
		puckPickupMessages = 0;
	}
#endif

	if(!((cond & G_IO_IN) && controller.reader.doRead(&type, &len, &buffer))) {
    return true;
  }

  static bool gotTeam = false;
	// this should be the only type of messages
	switch (type) {
	case MSG_WORLDINFO: {
		// Should be the first message we recieve from the controller.
		// Let's us calculate the number of robots on each team. We send
		// our ClaimTeam message here.
		WorldInfo worldinfo;
		worldinfo.ParseFromArray(buffer, len);
		int robotSize = worldinfo.robot_size();
		bool sameTeam = true;
		robotsPerTeam = 0; // global var

		// Count number of robots per team
		for (int i = 0; i < robotSize && sameTeam; i++) {
			if (worldinfo.robot(i).team() == 0) {
				robotsPerTeam++;
			} else {
				sameTeam = false;
			}
		}
		cout << "Got worldinfo! Calculated " << robotsPerTeam << " robots on each team."<<endl;

		//create the client viewer GUI
		if (runClientViewer)
			viewer->initClientViewer(robotsPerTeam, myTeam, CVDRAWFACTOR );

		// Claim our team
		ClaimTeam claimteam;
		claimteam.set_id(myTeam);
		controller.queue.push(MSG_CLAIMTEAM, claimteam);
		cout << "Generating ClaimTeam message for team ID #" << myTeam << endl;

		break;
	}
	case MSG_CLAIMTEAM: {
    // Received a ClaimTeam response message from the controller in response
    // to our asking for a team. If we got the team, then prepare the local
    // data structures and set simulationStarted to true. If we didn't get
    // the team, then die.
		ClaimTeam claimteam;
		claimteam.ParseFromArray(buffer, len);

			if (claimteam.granted()) {
				myTeam = claimteam.id();
				cout << "ClaimTeam: Success! We control team #" << myTeam << endl;
        gotTeam = true;

				ownRobots = new OwnRobot*[robotsPerTeam];
				for (int i = 0; i < robotsPerTeam; i++) {
					// We don't have any initial robot data, yet.
					ownRobots[i] = new OwnRobot();
					ownRobots[i]->index = i;
				}

				simulationStarted = true;

				//get that HOME info!
				for(int i = 0; i < claimteam.homes_size(); i++)
				{
				  int index = robotIdToIndex(claimteam.homes(i).id());

				  ownRobots[index]->homeRelX = claimteam.homes(i).relx();
				  ownRobots[index]->homeRelY = claimteam.homes(i).rely();
				}

			} else { // claimteam.granted() == false
				myTeam = -1;
				cout << "Client controls no teams!\n";
			}

			initializeRobots(controller);

		break;
	}
	case MSG_TIMESTEPUPDATE: {
    // Simulation is now on a new timestep. Simulate all robot movements.
    // Allow the user AI code to execute. If we are just starting up, then
    // run initializeRobots().
    if (!simulationStarted)
      break;

		TimestepUpdate timestep;
		timestep.ParseFromArray(buffer, len);
		currentTimestep = timestep.timestep();

		  if(currentTimestep % 2 == 0)
		  {
			  // Update all current positions.
			  for (int i = 0; i < robotsPerTeam; i++) {
			    //so far the simulation can handle actions up to every 2 sim steps smoothly. This probably doesn't scale.
			    //We'll need to implement the optimizations we talked about to improve this

				  if (ownRobots[i]->pendingCommand) {
					  // If we've waited too long for an update, send
					  // new ClientRobot messages.
					  // TODO: Lower from 100.
					  ownRobots[i]->pendingCommand = false;
					  timeoutMessages++;
				  }

				  // Update rel distance of our home.
				  ownRobots[i]->homeRelX -= ownRobots[i]->vx;
				  ownRobots[i]->homeRelY -= ownRobots[i]->vy;

          //Let the AI do this work
          // Simulate our new angle if we are turning.
          //estimateRotation(ownRobots[i]);

				  // Update rel distance of seenRobots.
				  //double minDistance = 9000.01;
				  double tempDistance;
				  for (vector<SeenRobot*>::iterator it = ownRobots[i]->seenRobots.begin(); it
						  != ownRobots[i]->seenRobots.end(); it++) {
					  (*it)->relx += (*it)->vx - ownRobots[i]->vx;
					  (*it)->rely += (*it)->vy - ownRobots[i]->vy;
					  tempDistance = relDistance((*it)->relx, (*it)->rely);
					  if (!Sees((*it)->relx, (*it)->rely)) {
						  // We can't see the robot anymore. Delete.
						  delete *it;
						  it = ownRobots[i]->seenRobots.erase(it);
						  it--; // Compensates for it++ in for loop.
					  } 
				  }

				  // Update rel distance of seenPucks.
				  for (vector<SeenPuck*>::iterator it = ownRobots[i]->seenPucks.begin(); it
						  != ownRobots[i]->seenPucks.end(); it++) {
					  (*it)->relx -= ownRobots[i]->vx;
					  (*it)->rely -= ownRobots[i]->vy;
					  tempDistance = relDistance((*it)->relx, (*it)->rely);

					  if (!Sees((*it)->relx, (*it)->rely)) {
						  // We can't see the puck anymore. Delete.
						  delete *it;
						  it = ownRobots[i]->seenPucks.erase(it);
						  it--; // Compensates for it++ in for loop.
					  } 
				  }

				  //robot AIs SHOULD execute every timestep
				  if(!ownRobots[i]->pendingCommand){
					  //if(currentTimestep - ownRobots[i]->whenLastSent > 10){
					  if((currentTimestep / 2) % 10 == myTeam % 10){
					    executeAi(ownRobots[i], i, controller);
					    //initializeRobots(controller); // Debug function call.
					  }
				  }

				  //clear the collided flag so that the client, or ai, or whatever is refusing to forget about collisions after a period doesn't die forever
				  ownRobots[i]->hasCollided = false;
			  }

			    //update the view of the viewed robot
			    if (runClientViewer)
			    {
			    	    gettimeofday(&timeCache, NULL);
				    if( viewer->getViewedRobot() != -1 && (timeCache.tv_sec*1000000 + timeCache.tv_usec) > (microTimeCache.tv_sec*1000000 + microTimeCache.tv_usec)+DRAWTIME)
				    {
					    viewer->updateViewer(ownRobots[viewer->getViewedRobot()]);
					    microTimeCache = timeCache;
				    }
			    }
			}



		break;
	}

	case MSG_SERVERROBOT: {
    // Received a ServerRobot message for either our robot, or an enemy
    // robot that we can see. Update our local data accordingly.
    if (!simulationStarted)
      break;

		ServerRobot serverrobot;
		serverrobot.ParseFromArray(buffer, len);

		receivedMessages++;

			int robotId = serverrobot.id();
			if (weControlRobot(robotId)) {
				// The serverrobot is from our team.
				int index = robotIdToIndex(robotId);
				ownRobots[index]->pendingCommand = false;

				if (serverrobot.has_velocityx())
				{
					ownRobots[index]->vx = serverrobot.velocityx();
				}
				if (serverrobot.has_velocityy())
				{
					ownRobots[index]->vy = serverrobot.velocityy();
				}
				//if (serverrobot.has_angle())
				//	ownRobots[index]->angle = serverrobot.angle();
				
				//set the angle based on the velocity
				float newangle = hack_angleFromVelocity(ownRobots[index]->vx, -1*ownRobots[index]->vy);
				if(newangle != -1)
				  ownRobots[index]->angle = newangle;
				
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
					// The serverrobot may or may not be on our team. Can we see it?
					index = robotIdToIndex(serverrobot.seesserverrobot(i).seenbyid());
					if (serverrobot.seesserverrobot(i).viewlostid()) {
            // Deprecated. Client now calculates when it loses sight! 

            /*
						// Could see before, can't see anymore.
						for (vector<SeenRobot*>::iterator it = ownRobots[index]->seenRobots.begin(); it
								!= ownRobots[index]->seenRobots.end(); it++) {
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
            */
					} else {
						// Can see. Add, or update?
						bool foundRobot = false;
						for (vector<SeenRobot*>::const_iterator it = ownRobots[index]->seenRobots.begin(); it
								!= ownRobots[index]->seenRobots.end() && !foundRobot; it++) {

							if ((*it)->id == serverrobot.id()) {
								foundRobot = true;
								//cout << "Our #" << index << " update see "
								//     << serverrobot.id() << " at relx: "
								//     << serverrobot.seesserverrobot(i).relx() << endl;
								if (serverrobot.has_velocityx() && (*it)->vx != serverrobot.velocityx()) {
									(*it)->vx = serverrobot.velocityx();
								}
								if (serverrobot.has_velocityy() && (*it)->vy != serverrobot.velocityy()) {
									(*it)->vy = serverrobot.velocityy();
								}
								/*if (serverrobot.has_angle() && (*it)->angle != serverrobot.angle()) {
									(*it)->angle = serverrobot.angle();
								}*/
								float newangle = hack_angleFromVelocity((*it)->vx, -1*(*it)->vy);
				        if(newangle != -1)
				          (*it)->angle = newangle;
				          
								if (serverrobot.has_haspuck() && (*it)->hasPuck != serverrobot.haspuck()) {
									(*it)->hasPuck = serverrobot.haspuck();
								}
								if (serverrobot.has_hascollided() && (*it)->hasCollided != serverrobot.hascollided()) {
									(*it)->hasCollided = serverrobot.hascollided();
								}
								if (serverrobot.seesserverrobot(i).has_relx())
									(*it)->relx = serverrobot.seesserverrobot(i).relx();
								if (serverrobot.seesserverrobot(i).has_rely())
									(*it)->rely = serverrobot.seesserverrobot(i).rely();
								(*it)->lastTimestepSeen = currentTimestep;
							}
						}

						if (!foundRobot) {

							SeenRobot *r = new SeenRobot();
							//cout << "Our #" << index << " begin see "
							//     << serverrobot.id() << endl;
							if (serverrobot.has_velocityx())
							{
								r->vx = serverrobot.velocityx();
							}
							if (serverrobot.has_velocityy())
							{
								r->vy = serverrobot.velocityy();
							}
							/*if (serverrobot.has_angle())
								r->angle = serverrobot.angle();*/
							float newangle = hack_angleFromVelocity(r->vx, -1*r->vy);
				      if(newangle != -1)
				        r->angle = newangle;
				  
							if (serverrobot.has_haspuck())
								r->hasPuck = serverrobot.haspuck();
							if (serverrobot.has_hascollided())
								r->hasCollided = serverrobot.hascollided();
							r->id = serverrobot.id();

							if (serverrobot.seesserverrobot(i).has_relx())
  							r->relx = serverrobot.seesserverrobot(i).relx();
							if (serverrobot.seesserverrobot(i).has_rely())
  							r->rely = serverrobot.seesserverrobot(i).rely();

							r->lastTimestepSeen = currentTimestep;
							ownRobots[index]->seenRobots.push_back(r);
						}
					}
				}
			}

		break;
	}
	case MSG_PUCKSTACK: {
    // Received a PuckStack message for a puck that has either
    // newly come into view, or had a stacksize change. Update
    // the puck lists for our robots.
    if (!simulationStarted)
      break;

		PuckStack puckstack;
		puckstack.ParseFromArray(buffer, len);

		int index;
		int listSize = puckstack.seespuckstack_size();
		SeenPuck* deleteStack = NULL;
		for (int i = 0; i < listSize; i++) {
			// Check if one of our robots is in the list. If it is,
			// add the puck to its seenPuck list. Pretend that every
			// puck is a new, previously-unseen puck.
			if (weControlRobot(puckstack.seespuckstack(i).seenbyid())) {
				index = robotIdToIndex(puckstack.seespuckstack(i). seenbyid()); // Our robot that can see.
				// Look through seenPuck list, check if the new puck's
				// position is the same as one we already see. If so,
				// update our stacksize.
				bool foundPuck = false;
				for (vector<SeenPuck*>::iterator it = ownRobots[index]->seenPucks.begin(); it
						!= ownRobots[index]->seenPucks.end() && !foundPuck; it++) {
					if ((*it)->xid == puckstack.x() && (*it)->yid == puckstack.y()) {

					  if((*it)->stackSize > (int)puckstack.stacksize())
					  {
					    //check if our robot has picked it up
					    //if(abs(puckstack.seespuckstack(i).relx()) < ROBOTDIAMETER/2 && abs(puckstack.seespuckstack(i).rely()) < ROBOTDIAMETER/2){				    
					    if(puckstack.robotmover() == indexToRobotId(index))
					    	ownRobots[index]->hasPuck = true;
					  }
					  if((*it)->stackSize < (int)puckstack.stacksize())
					  {
					    //check if our robot has dropped it
					    //if(abs(puckstack.seespuckstack(i).relx()) < ROBOTDIAMETER/2 && abs(puckstack.seespuckstack(i).rely()) < ROBOTDIAMETER/2){
					    if(puckstack.robotmover() == indexToRobotId(index))
					    	ownRobots[index]->hasPuck = false;

					  }

						(*it)->stackSize = puckstack.stacksize();
						foundPuck = true;
						if ((*it)->stackSize <= 0 || puckstack.seespuckstack(i).viewlostid()) {
						  deleteStack = (*it);
							//lol DONT delete it here. What if other people on our team are watching it too? They won't be able to forget it. LOL
							it = ownRobots[index]->seenPucks.erase(it);
						  it--; // Compensates for it++ in for loop.
						}
					}
				}

				// If we didn't find a match in seenPucks, add new puck.
				if (!foundPuck && puckstack.stacksize() > 0) {
					SeenPuck *p = new SeenPuck();
					p->stackSize = puckstack.stacksize();
					p->relx = puckstack.seespuckstack(i).relx();
					p->rely = puckstack.seespuckstack(i).rely();
					p->xid = puckstack.x();
					p->yid = puckstack.y();

			    //check if our robot has dropped it, with a backup
			    if(puckstack.robotmover() == indexToRobotId(index))
			    	ownRobots[index]->hasPuck = false;

					ownRobots[index]->seenPucks.push_back(p);
				}
			}
		}
		
		if(deleteStack != NULL)
		  delete deleteStack;

		break;
	}
	default:
		cerr << "Unknown message!" << endl;
		break;
	}

  //int mss = net::get_mss(g_io_channel_unix_get_fd(ioch));
  if(!writing && controller.queue.remaining()) {
    g_source_remove(gwatch);
    // Ensure that we're watching for writability
    gwatch = g_io_add_watch(ioch, (GIOCondition)(G_IO_IN | G_IO_OUT), runner, data);
    writing = true;
  }

	return true;
}

//basic connection initializations for the client
void Client::initClient(int argc, char* argv[], string pathToExe, bool runClientViewer) {

	if( !gtk_init_check(&argc, &argv) ){
		cerr<<"Unable to initialize the X11 windowing system. Client Viewer will not work!"<<endl;
		runClientViewer=false;
	}
	g_type_init();

	int controllerfd = -1;
	int currentController = rand() % controllerips.size();

	while (controllerfd < 0) {
		cout << "Attempting to connect to controller " << controllerips.at(currentController) << "..." << flush;
		controllerfd = net::do_connect(controllerips.at(currentController).c_str(), CLIENTS_PORT, CONNECT_TIMEOUT);

		if (0 > controllerfd) {
			throw SystemError("Failed to connect to controller");
		} else if (0 == controllerfd) {
			throw runtime_error("Invalid controller address");
		}
		currentController = rand() % controllerips.size();
	}
	net::set_blocking(controllerfd, false);

	cout << " connected." << endl;

	net::connection controller(controllerfd, net::connection::CONTROLLER);
	ClientViewer* viewer = new ClientViewer(pathToExe);

	if(runClientViewer){
		gettimeofday(&microTimeCache, NULL);
	}

	//all the variables that we want to read in the controller message handler ( see bellow ) need to be either passed in this struct or delcared global
	passToRun passer(runClientViewer, controller, viewer, this);

	//this calls the function "run" everytime we get a message from the
	//"controllerfd"
	GIOChannel *ioch = g_io_channel_unix_new(controllerfd);
	gwatch = g_io_add_watch(ioch, G_IO_IN, runner, (gpointer)&passer);

	gtk_main();
}

////////////////////////////////////////////////////////////////////////
// Own Robot
////////////////////////////////////////////////////////////////////////
void OwnRobot::destAppend(pair<double, double>& p) {
	destinations.push_back(p);
}

void OwnRobot::destPush(pair<double, double>& p) {
	destinations.push_front(p);
}

pair<double, double>& OwnRobot::destFront() {
	return destinations.front();
}

void OwnRobot::destPopFront() {
	destinations.pop_front();
}

bool OwnRobot::hasDestination() {
	return destinations.size() > 0;
}

void OwnRobot::clearDestinations() {
	destinations.clear();
}

////////////////////////////////////////////////////////////////////////
// Main Loop
////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
	bool runClientViewer = false;
	srand( time(NULL));
	string pathToExe=argv[0];
	pathToExe=pathToExe.substr(0, pathToExe.find_last_of("//") + 1);

	cout << "--== Client Software ==-" << endl;

	////////////////////////////////////////////////////
	cout << "Client Initializing ..." << endl;
	Client c = Client();
	helper::CmdLine cmdline(argc, argv);

	const char* configFileName = cmdline.getArg("-c", "config").c_str();
	cout << "Using config file: " << configFileName << endl;
	c.loadConfigFile(configFileName, pathToExe);

	runClientViewer = cmdline.getArg("-viewer").length() ? true : false;
	cout << "Started client with the client viewer set to " << runClientViewer << endl;

	if(cmdline.getArg("-l").length()) {
		string newcontrollerip = cmdline.getArg("-l");
		c.setControllerIp(newcontrollerip);
	}

	int myTeam = strtol(cmdline.getArg("-t", "0").c_str(), NULL, 0);
	cout << "Trying to control team #" << myTeam << " (use -t <team> to change)" << endl;
	c.setMyTeam(myTeam);
	////////////////////////////////////////////////////

	cout << "Client Running!" << endl;
	c.initClient(argc, argv, pathToExe, runClientViewer);

	cout << "Client Shutting Down ..." << endl;

	cout << "Goodbye!" << endl;

	return 0;
}
