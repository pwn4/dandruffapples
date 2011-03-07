/*/////////////////////////////////////////////////////////////////////////////////////////////////
Client program
This program communications with controllers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/
#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <string>
#include <stdlib.h>
#include <fstream>

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

#include <gtk/gtk.h>

#include "../common/helper.h"

using namespace std;
using namespace google;
using namespace protobuf;

enum EventType {
  EVENT_CLOSEST_ROBOT_STATE_CHANGE,
  EVENT_NEW_CLOSEST_ROBOT,
  EVENT_START_SEEING_PUCKS,
  EVENT_END_SEEING_PUCKS,
  EVENT_CAN_PICKUP_PUCK,
  EVENT_NEAR_PUCK,
  EVENT_NOT_MOVING,
  EVENT_MAX
};

class SeenHome {
public:
  float relx;
  float rely;
  int teamId;

  SeenHome() : relx(0.0), rely(0.0), teamId(-1) {}
};

class SeenPuck {
public:
  float relx;
  float rely;
  int stackSize;

  SeenPuck() : relx(0.0), rely(0.0), stackSize(1) {}
};

class Robot {
public:
  float vx;
  float vy;
  float angle;
  bool hasPuck;
  bool hasCollided;

  Robot() : vx(0.0), vy(0.0), angle(0.0), hasPuck(false),
            hasCollided(false) {}
};

class SeenRobot : public Robot {
public:
  int id;
  int lastTimestepSeen;
  bool viewable;
  float relx;
  float rely;

  SeenRobot() : Robot(), id(-1), lastTimestepSeen(-1), viewable(true),
      relx(0.0), rely(0.0) {}
};

class OwnRobot : public Robot {
public:
  bool pendingCommand;
  int whenLastSent;
  int closestRobotId;
  int behaviour;
  vector<SeenRobot*> seenRobots;
  vector<SeenPuck*> seenPucks;
  SeenHome* myHome;
  vector<SeenHome*> seenHomes;
  vector<EventType> eventQueue;

  OwnRobot() : Robot(), pendingCommand(false), whenLastSent(-1),
      closestRobotId(-1), behaviour(-1), myHome(NULL) {}
};

class ClientRobotCommand {
public:
  bool sendCommand;
  bool changeVx;
  float vx;
  bool changeVy;
  float vy;
  bool changeAngle;
  float angle;
  bool changePuckPickup; 
  bool puckPickup;

  ClientRobotCommand() : sendCommand(false), changeVx(false), vx(0.0),
      changeVy(false), vy(0.0), changeAngle(false), angle(0.0), 
      changePuckPickup(false), puckPickup(false) {}
};

class ClientViewer {
private:
	ofstream debug;
	int viewedRobot;
	string builderPath;
	GtkBuilder *builder;

public:
	void initClientViewer(int);
	void updateViewer(OwnRobot* ownRobot);
	int getViewedRobot() { return viewedRobot; }

	ClientViewer(int, char**);
	~ClientViewer();
};

struct passToRun{
	bool runClientViewer;
	WorldInfo worldinfo;
	TimestepUpdate timestep;
	ServerRobot serverrobot;
	ClaimTeam claimteam;
	net::connection controller;
	ClientViewer* viewer;

	passToRun( bool _runClientViewer, WorldInfo _worldinfo, TimestepUpdate _timestep, ServerRobot _serverrobot, ClaimTeam _claimteam, net::connection _controller, ClientViewer* _viewer) :
		runClientViewer(_runClientViewer), worldinfo(_worldinfo), timestep(_timestep), serverrobot(_serverrobot), claimteam(_claimteam), controller(_controller), viewer(_viewer){};
};

#endif
