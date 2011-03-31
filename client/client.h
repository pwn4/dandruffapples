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
#include <map>
#include <list>
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

#include "variant.h"

using namespace std;
using namespace google;
using namespace protobuf;

const double PI = 3.1415926535897932;

class Client;
class ClientAi;

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
	vector<SeenRobot*> seenRobots;
	vector<SeenPuck*> seenPucks;
	double homeRelX;
	double homeRelY;
	double desiredAngle;

	bool has_path_home;
	bool focus; // when focus on the next destination, won't change destination

	int index; // this is for AI memory
	ClientAi* ai; // user-defined ai code

	// Patrolling support
	void destPush(pair<double, double>& p); // push to the next visit point
	void destAppend(pair<double, double>& p); // append to the end of list
	pair<double, double>& destFront(); // get the next destination
	void destPopFront(); // remove the next destination
	bool hasDestination(); // return true if has destination
	void clearDestinations();

	OwnRobot() :
		Robot(), pendingCommand(false), whenLastSent(-1), closestRobotId(-1), homeRelX(0.0),
				homeRelY(0.0), desiredAngle(0.0), has_path_home(false), focus(false), ai(NULL) {
		destinations.clear();
	}
protected:
	pair<double, double> temp_destination; // temporary destination, overrides destinations (used for resolving collision?)
	list<pair<double, double> > destinations; // a series of points that robot visits one by one
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
  void estimateRotation(OwnRobot* ownRobot);
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

  static double verifyAngle(double angle);
};

class ClientAi : public Client{
private:
	map<string, variant> knowledge;
public:
    virtual void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) = 0;
    // Helpers

    // Team knowledge
    template<typename T> void setTeamKnowledge(string key, T value) {
    	knowledge[key] = variant(value);
    }
    template<typename T> T getTeamKnowledge(string key) {
    	return knowledge[key];
    }
    template<typename T> const T & getTeamKnowledgeRef(string key) {
    	return knowledge[key].get<T>();
    }
    void eraseTeamKnowledge(string key) {
    	knowledge.erase(key);
    }
    bool hasTeamKnowledge(string key) {
    	return knowledge.find(key) != knowledge.end();
    }

    SeenPuck* findPickUpablePuck(OwnRobot* ownRobot);
    SeenPuck* findClosestPuck(OwnRobot* ownRobot);
	SeenPuck* findSecondClosestPuck(OwnRobot* ownRobot);

    SeenRobot* findClosestRobot(OwnRobot* ownRobot);
	SeenRobot* robotClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);
	SeenRobot* robotSecondClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);

	bool ImClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);
	bool ImSecondClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck);
	bool canPickUpPuck(OwnRobot* ownRobot);
	bool closeToHome(OwnRobot* ownRobot, double dist);
    bool insideOurHome(OwnRobot* ownRobot);
	bool isEnemy(SeenRobot* seenRobot);
	int canSeeNumPucks(OwnRobot* ownRobot);

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
	static pair<double, double> polar2cartesian(double rho, double theta);
	static double degree2radian(double d);
	static pair<double, double> normalize(pair<double, double> coords, double scale);
	static double distance(double x1, double y1, double x2, double y2);
};

#endif
