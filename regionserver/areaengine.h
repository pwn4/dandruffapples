#ifndef _AREAENGINE_H_
#define _AREAENGINE_H_

#include "../common/except.h"
#include "../common/globalconstants.h"
#include <algorithm>
#include <iostream>
#include <limits.h>
#include <algorithm>
#include <sys/time.h>
#include <math.h>
#include <cstddef>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <set>
#include <queue>
#include <string>

#include "../common/claim.pb.h"
#include "../common/regionupdate.pb.h"
#include "../common/net.h"
#include "../common/helper.h"
#include "../common/regionrender.pb.h"

using namespace std;
using namespace net;

#define TOP_LEFT 0
#define TOP 1
#define TOP_RIGHT 2
#define RIGHT 3
#define BOTTOM_RIGHT 4
#define BOTTOM 5
#define BOTTOM_LEFT 6
#define LEFT 7

struct Index{
  int x, y;

  Index() : x(0), y(0) {}
  Index(int newx, int newy) : x(newx), y(newy) {}
};

struct RobotObject{
  int id, lastStep;
  double x, y;
  double angle;
  double vx, vy;
  Index arrayLocation;
  time_t lastCollision;
  map<int, bool> lastSeenBy;
  RobotObject * nextRobot;
  int controllerfd;
  int team;
  bool holdingPuck;

  RobotObject(int newid, double newx, double newy, double newa, Index aLoc, int curStep, int teamId, bool hasPuck) : id(newid), lastStep(curStep-1), x(newx), y(newy), angle(newa), vx(0), vy(0), arrayLocation(aLoc), lastCollision(time(NULL)), nextRobot(NULL), controllerfd(-1), team(teamId), holdingPuck(hasPuck) {}
  RobotObject(int newid, double newx, double newy, double newa, double newvx, double newvy, Index aLoc, int curStep, int teamId, bool hasPuck) : id(newid), lastStep(curStep-1), x(newx), y(newy), angle(newa), vx(newvx), vy(newvy), arrayLocation(aLoc), lastCollision(time(NULL)), nextRobot(NULL), controllerfd(-1), team(teamId), holdingPuck(hasPuck) {}
};

struct PuckStackObject{
  int x, y;
  int count;
  vector<RobotObject*> seenBy;

  PuckStackObject * nextStack;

  PuckStackObject(int newx, int newy) : x(newx), y(newy), count(1), nextStack(NULL) {}
};

struct ArrayObject{
  PuckStackObject * pucks;
  RobotObject * robots;

  ArrayObject() : pucks(NULL), robots(NULL) {}
};

class ComparePuckStackObject {
    public:
    bool operator()(PuckStackObject* const &r1, PuckStackObject* const &r2); // Returns true if t1 is earlier than t2
};

struct Command {
  int robotId;
  double velocityx, velocityy;
  double angle;
  int step;
  int puckAction; //0=nothing, 1=pickup, 2=drop

  Command() : robotId(-1), velocityx(INT_MAX), velocityy(INT_MAX), angle(INT_MAX), step(-1), puckAction(0) {}
};

class CompareCommand {
    public:
    bool operator()(Command* const &r1, Command* const &r2); // Returns true if t1 is earlier than t2
};


class AreaEngine {
protected:
int robotRatio, regionRatio;    //robotDiameter:puckDiameter, regionSideLength:puckDiameter
double pickupRange;
int regionBounds;
int elementSize;  //in pucks
double viewDist, viewAng;
int coolDown; //cooldown before being able to change velocities
double maxSpeed, maxRotate; //a bound on the speed of robots. Should be passed by the clock.
ArrayObject** robotArray;
map<int, RobotObject*> robots;
map<int, ServerRobot*> updates;
EpollConnection ** neighbours;
RegionUpdate regionUpdate[8];
vector<EpollConnection*> controllers;
priority_queue<Command*, vector<Command*>, CompareCommand> serverChangeQueue;
priority_queue<Command*, vector<Command*>, CompareCommand> clientChangeQueue;
//for rendering
map<PuckStackObject*, bool, ComparePuckStackObject> puckq;
//for collision
double sightSquare, collisionSquare;

  void BroadcastRobot(RobotObject *curRobot, Index oldIndices, Index newIndices, int step, RegionUpdate * regionHandle);
  void BroadcastPuckStack(PuckStackObject *curStack, RegionUpdate * regionHandle);

public:
  RegionRender render;
  int curStep;
  Index getRobotIndices(double x, double y, bool clip);

  void Step(bool generateImage);

  bool Sees(double x1, double y1, double x2, double y2);

  bool Collides(double x1, double y1, double x2, double y2);

  void PickUpPuck(int robotId);
  void DropPuck(int robotId);
  PuckStackObject* AddPuck(double newx, double newy, int RobotId);
  PuckStackObject* AddPuck(double newx, double newy);
  bool RemovePuck(double x, double y, int RobotId);
  void SetPuckStack(double newx, double newy, int newc);

  void AddRobot(RobotObject * oldRobot);
  RobotObject* AddRobot(int robotId, double newx, double newy, double newa, double newvx, double newvy, int atStep, int teamId, bool hasPuck, bool broadcast);

  void RemoveRobot(int robotId, int xInd, int yInd, bool freeMem);

  AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle, double maximumSpeed, double maximumRotate);
  ~AreaEngine();

  bool WeControlRobot(int robotId);

  //Robot modifiers
  bool ChangeVelocity(int robotId, double newvx, double newvy); //Allows strafing, if we may want it
  bool ChangeAngle(int robotId, double newangle);  //Allows turning
  //note, I could have added a changeMovement function that gets the robot to move like in Vaughn's code. However, it requires some trig calculations and such. Therein, it would be better if the clients' code (if it turns out we can't have strafing) restricts movement, and calculates the appropriate changeVelocity and changeAngle calls.

  void SetNeighbour(int placement, EpollConnection* socketHandle);

  void GotServerRobot(ServerRobot message);

  void GotPuckStack(PuckStack message);

  void AddController(EpollConnection* socketHandle);

  void flushNeighbours();

  void flushControllers();

  void forceUpdates();
  
  //new async methods
  void clearBuffers();
  
  void flushBuffers();

};

#endif
