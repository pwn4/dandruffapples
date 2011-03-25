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
#include "../common/regionupdate.pb.h"
#include "../common/timestep.pb.h"
#include "../common/worldinfo.pb.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagequeue.h"
#include "../common/messagereader.h"
#include "../common/except.h"

#include <gtk/gtk.h>

#include "../common/helper.h"
#include "../common/globalconstants.h"
#include "../worldviewer/drawer.h"

using namespace std;
using namespace google;
using namespace protobuf;

class Client;
class ClientAi;

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
	double relx;
	double rely;
	int stackSize;
	unsigned int xid, yid;

	SeenPuck() :
		relx(0.0), rely(0.0), stackSize(1), xid(0), yid(0) {
	}
};

class Robot {
public:
	double vx;
	double vy;
	double angle;
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
	double relx;
	double rely;

	SeenRobot() :
		Robot(), id(-1), lastTimestepSeen(-1), viewable(true), relx(0.0), rely(0.0) {
	}
};

class OwnRobot: public Robot {
public:
	bool pendingCommand;
	int whenLastSent;
	int closestRobotId;
	// int behaviour;
	vector<SeenRobot*> seenRobots;
	vector<SeenPuck*> seenPucks;
	double homeRelX;
	double homeRelY;
	vector<EventType> eventQueue;

	int index; // this is for AI memory
	ClientAi* ai; // user-defined ai code

	OwnRobot() :
		Robot(), pendingCommand(false), whenLastSent(-1), closestRobotId(-1), homeRelX(0.0),
				homeRelY(0.0), ai(NULL) {
	}
};

class ClientRobotCommand {
public:
	bool sendCommand;
	bool changeVx;
	double vx;
	bool changeVy;
	double vy;
	bool changeAngle;
	double angle;
	bool changePuckPickup;
	bool puckPickup;

	ClientRobotCommand() :
		sendCommand(false), changeVx(false), vx(0.0), changeVy(false), vy(0.0), changeAngle(false), angle(0.0),
				changePuckPickup(false), puckPickup(false) {
	}

  // Setters - set sendCommand, boolean, and change value at once
  void setVx(double _vx) {
    sendCommand = true;
    changeVx = true;
    vx = _vx;
  }  

  void setVy(double _vy) {
    sendCommand = true;
    changeVy = true;
    vy = _vy;
  }  

  void setAngle(double _angle) {
    sendCommand = true;
    changeAngle = true;
    angle = _angle;
  }  

  void setPuckPickup(bool _puckPickup) {
    sendCommand = true;
    changePuckPickup = true;
    puckPickup = _puckPickup;
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
	void initClientViewer(int, int, int);
	void updateViewer(OwnRobot* ownRobot);
	int getViewedRobot() {
		return viewedRobot;
	}

	ClientViewer(string);
	~ClientViewer();
};

struct passToRun {
	bool runClientViewer;

	net::connection controller;
	ClientViewer* viewer;
	Client* client;

	passToRun(bool _runClientViewer, net::connection _controller, ClientViewer* _viewer, Client* _client) :
		runClientViewer(_runClientViewer), controller(_controller), viewer(_viewer), client(_client) {
	}
	;
};

struct passToDrawingAreaExpose {
	int myTeam;
	int numberOfRobots;
	int *drawFactor;
	bool *draw;
	OwnRobot* ownRobotDraw;
	GtkToggleToolButton *info;
	GtkBuilder *builder;

	passToDrawingAreaExpose(int _myTeam, int _numberOfRobots, int *_drawFactor, bool* _draw, OwnRobot* _ownRobotDraw,
			GtkToggleToolButton *_info, GtkBuilder *_builder) :
		myTeam(_myTeam), numberOfRobots(_numberOfRobots), drawFactor(_drawFactor), draw(_draw),
				ownRobotDraw(_ownRobotDraw), info(_info), builder(_builder) {
	}
	;
};

struct passToZoom {
	int *drawFactor;
	GtkWidget *mainWindow;
	GtkDrawingArea *drawingArea;
	GtkToolButton *zoomIn;
	GtkToolButton *zoomOut;

	passToZoom(int *_drawFactor, GtkWidget *_mainWindow, GtkDrawingArea *_drawingArea, GtkToolButton *_zoomIn,
			GtkToolButton *_zoomOut) :
		drawFactor(_drawFactor), mainWindow(_mainWindow), drawingArea(_drawingArea), zoomIn(_zoomIn), zoomOut(_zoomOut) {
	}
	;
};

struct passToQuit {
	int *viewedRobot;
	GtkWidget *mainWindow;

	passToQuit(int *_viewedRobot, GtkWidget *_mainWindow) :
		viewedRobot(_viewedRobot), mainWindow(_mainWindow) {
	}
	;
};

class Client {
private:
	// For Client viewer update
	struct timeval timeCache, microTimeCache;
	//Config variables
	vector<string> controllerips; //controller IPs
	// my team id
	int myTeam;
	void executeAi(OwnRobot* ownRobot, int index, net::connection &controller);
	void initializeRobots(net::connection & controller);
	guint gwatch;
	bool writing;
	double relDistance(double x1, double y1);
	bool sameCoordinates(double x1, double y1, double x2, double y2);
	vector< pair<ClientAi*, int> > clientAiList;
protected:
	// Stat variables
	time_t lastSecond;
	int sentMessages;
	int receivedMessages;
	int pendingMessages;
	int timeoutMessages;
	int puckPickupMessages;
	// Robots
	OwnRobot** ownRobots;
	// Helper functions
	unsigned int indexToRobotId(int index);
	int robotIdToIndex(int robotId);
public:
	Client() : writing(false),
			   lastSecond(time(NULL)),
			   sentMessages(0),
			   receivedMessages(0),
			   pendingMessages(0),
			   timeoutMessages(0),
			   puckPickupMessages(0) {};
	void initClient(int argc, char* argv[], string pathToExe, bool runClientViewer);
	void loadConfigFile(const char* configFileName, string& pathToExe);
	void setControllerIp(string newcontrollerip);
	void setMyTeam(int myTeam);
	bool weControlRobot(int robotId);
	gboolean run(GIOChannel *ioch, GIOCondition cond, gpointer data);
	//TODO: user-defined AI code
	ClientRobotCommand userAiCode(OwnRobot* ownrobot);

};

class ClientAi {
public:
    virtual void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) = 0;
    // Helpers

    SeenPuck* findPickUpablePuck(OwnRobot* ownRobot);
    SeenPuck* findClosestPuck(OwnRobot* ownRobot);
		SeenPuck* findSecondClosestPuck(OwnRobot* ownRobot);
    SeenRobot* findClosestRobot(OwnRobot* ownRobot);
		SeenRobot* robotClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);
		bool ImClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);
    bool hasCanPickUpPuckEvent(OwnRobot* ownRobot);
    bool hasNotMovingEvent(OwnRobot* ownRobot);
		bool canPickUpPuck(OwnRobot* ownRobot);
		bool closeToHome(OwnRobot* ownRobot);
    bool insideOurHome(OwnRobot* ownRobot);
		int numRobotsLeftOfMe(OwnRobot* ownRobot);
		int numRobotsRightOfMe(OwnRobot* ownRobot);
		int numRobotsTopOfMe(OwnRobot* ownRobot);
		int numRobotsBottomOfMe(OwnRobot* ownRobot);
		int canSeeNumRobots(OwnRobot* ownRobot);
		SeenRobot* leftmostRobotToMe(OwnRobot* ownRobot);
		SeenRobot* rightmostRobotToMe(OwnRobot* ownRobot);
		SeenRobot* topmostRobotToMe(OwnRobot* ownRobot);
		SeenRobot* bottommostRobotToMe(OwnRobot* ownRobot);

		SeenRobot* closestRobotDirection(OwnRobot* ownRobot, int direction);
		int numRobotsDirection(OwnRobot*, int direction);
		bool puckInsideOurHome(SeenPuck* seenPuck, OwnRobot* ownRobot);
    static double relDistance(double x1, double y1);
		static bool sameCoordinates(double x1, double y1, double x2, double y2);
};

#endif
