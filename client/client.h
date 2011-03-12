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
#include <math.h>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <sys/time.h>

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
#include "../common/imageconstants.h"
#include "../worldviewer/drawer.h"

using namespace std;
using namespace google;
using namespace protobuf;

//constants for drawing as taken from the regionserver.cpp line ~216
//apparently in the future they will be set by the clock server
#define ROBOTDIAMETER 4
#define PUCKDIAMETER 1
#define VIEWDISTANCE 20
#define DRAWFACTOR 10

#define ZOOMSPEED 1
#define MINZOOMED 5
#define MAXZOOMED 20

//draw every DRAWTIME microseconds
#define DRAWTIME 0

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

class SeenPuck {
public:
	float relx;
	float rely;
	int stackSize;

	SeenPuck() :
		relx(0.0), rely(0.0), stackSize(1) {
	}
};

class Robot {
public:
	float vx;
	float vy;
	float angle;
	bool hasPuck;
	bool hasCollided;

	Robot() :
		vx(0.0), vy(0.0), angle(0.0), hasPuck(false), hasCollided(false) {
	}
};

class SeenRobot: public Robot {
public:
	unsigned int id;
	int lastTimestepSeen;
	bool viewable;
	float relx;
	float rely;

	SeenRobot() :
		Robot(), id(-1), lastTimestepSeen(-1), viewable(true), relx(0.0), rely(0.0) {
	}
};

class OwnRobot: public Robot {
public:
	bool pendingCommand;
	int whenLastSent;
	int closestRobotId;
	int behaviour;
	vector<SeenRobot*> seenRobots;
	vector<SeenPuck*> seenPucks;
	float homeRelX;
	float homeRelY;
	vector<EventType> eventQueue;

	OwnRobot() :
		Robot(), pendingCommand(false), whenLastSent(-1), closestRobotId(-1), behaviour(-1), homeRelX(0.0), homeRelY(
				0.0) {
	}
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

	ClientRobotCommand() :
		sendCommand(false), changeVx(false), vx(0.0), changeVy(false), vy(0.0), changeAngle(false), angle(0.0),
				changePuckPickup(false), puckPickup(false) {
	}
};

class ClientViewer {
private:
	ofstream debug;
	int viewedRobot, *drawFactor;
	string builderPath;
	GtkBuilder *builder;
	GtkDrawingArea *drawingArea;
	bool draw;
	OwnRobot ownRobotDraw;

public:
	void initClientViewer(int, int, int, int, int, int);
	void updateViewer(OwnRobot* ownRobot);
	int getViewedRobot() {
		return viewedRobot;
	}

	ClientViewer(char*);
	~ClientViewer();
};

struct passToRun {
	bool runClientViewer;

	net::connection controller;
	ClientViewer* viewer;

	passToRun(bool _runClientViewer, net::connection _controller, ClientViewer* _viewer) :
		runClientViewer(_runClientViewer), controller(_controller), viewer(_viewer) {
	}
	;
};

struct passToDrawingAreaExpose {
	int myTeam;
	int *drawFactor;
	int robotDiameter;
	int puckDiameter;
	int viewDistance;
	bool *draw;
	OwnRobot* ownRobotDraw;

	passToDrawingAreaExpose(int _myTeam, int *_drawFactor, int _robotDiameter, int _puckDiameter, int _viewDistance, bool* _draw,
			OwnRobot* _ownRobotDraw) :
		myTeam(_myTeam), drawFactor(_drawFactor), robotDiameter(_robotDiameter), puckDiameter(_puckDiameter), viewDistance(_viewDistance),
				draw(_draw), ownRobotDraw(_ownRobotDraw) {
	}
	;
};

struct passToZoom {
	int viewDistance;
	int robotDiameter;
	int *drawFactor;
	GtkWidget *mainWindow;
	GtkDrawingArea *drawingArea;
	GtkToolButton *zoomIn;
	GtkToolButton *zoomOut;

	passToZoom( int _viewDistance, int _robotDiameter, int *_drawFactor, GtkWidget *_mainWindow, GtkDrawingArea *_drawingArea, GtkToolButton *_zoomIn, GtkToolButton *_zoomOut) :
		viewDistance(_viewDistance), robotDiameter(_robotDiameter), drawFactor(_drawFactor),  mainWindow(_mainWindow), drawingArea(_drawingArea), zoomIn(_zoomIn), zoomOut(_zoomOut)  {
	}
	;
};

#endif
