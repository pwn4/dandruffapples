#ifndef _AREAENGINE_H_
#define _AREAENGINE_H_

#include "../common/except.h"
#include "../common/imageconstants.h"
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
#include <gtk/gtk.h>
#include <cairo.h>
#include <string>
#include "../common/serverrobot.pb.h"
#include "../common/net.h"

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

struct PuckStackObject{
  int x, y;
  int count;
  
  PuckStackObject * nextStack;
};

struct RobotObject{
  int id, lastStep;
  double x, y;
  double angle;
  double vx, vy;
  bool holdingPuck;
  Index arrayLocation;
  time_t lastCollision;
  map<int, bool> *lastSeen;
  string robotColor;
  RobotObject * nextRobot;
  int controllerfd;
  int team;
 
  
  RobotObject(int newid, double newx, double newy, double newa, Index aLoc, int curStep, string color) : id(newid), lastStep(curStep), x(newx), y(newy), angle(newa), vx(0), vy(0), arrayLocation(aLoc), lastCollision(time(NULL)), lastSeen(new map<int, bool>), robotColor(color), nextRobot(NULL), controllerfd(-1), team(0) {}
  RobotObject(int newid, double newx, double newy, double newa, double newvx, double newvy, Index aLoc, int curStep, string color) : id(newid), lastStep(curStep), x(newx), y(newy), angle(newa), vx(newvx), vy(newvy), arrayLocation(aLoc), lastCollision(time(NULL)), lastSeen(new map<int, bool>), robotColor(color), nextRobot(NULL), controllerfd(-1), team(0) {}
};

struct ArrayObject{
  PuckStackObject * pucks;
  RobotObject * robots;
  
  RobotObject * lastRobot;
  PuckStackObject * lastPuckStack;
  
  ArrayObject() : pucks(NULL), robots(NULL), lastRobot(NULL), lastPuckStack(NULL) {}
};

struct ColorObject{
  double r, g, b;
  
  ColorObject(int newr, int newg, int newb) : r(newr), g(newg), b(newb) {}
};


class AreaEngine {
protected:
int robotRatio, regionRatio;    //robotDiameter:puckDiameter, regionSideLength:puckDiameter
int regionBounds;
int elementSize;  //in pucks
double viewDist, viewAng;
int coolDown; //cooldown before being able to change velocities
int** puckArray;
double maxSpeed, maxRotate; //a bound on the speed of robots. Should be passed by the clock.
ArrayObject** robotArray;
map<int, RobotObject*> robots;
map<int, ServerRobot*> updates;
EpollConnection ** neighbours;
cairo_t *stepImageDrawer;

  ColorObject colorFromTeam(int teamId);
  
public:
  cairo_surface_t *stepImage; //contains the image of the last step called with generateImage=true
  int curStep;
  Index getRobotIndices(double x, double y, bool clip);
  
  void Step(bool generateImage);
  
  bool Sees(double x1, double y1, double x2, double y2);
  
  bool Collides(double x1, double y1, double x2, double y2);
  
  void AddRobot(RobotObject * oldRobot);
  RobotObject* AddRobot(int robotId, double newx, double newy, double newa, double newvx, double newvy, int atStep, string newColor);
  
  bool RemoveRobot(int robotId, int xInd, int yInd, bool freeMem);
  
  AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle, double maximumSpeed, double maximumRotate);
  ~AreaEngine();
  
  //Robot modifiers
  bool ChangeVelocity(int robotId, double newvx, double newvy); //Allows strafing, if we may want it
  bool ChangeAngle(int robotId, double newangle);  //Allows turning
  //note, I could have added a changeMovement function that gets the robot to move like in Vaughn's code. However, it requires some trig calculations and such. Therein, it would be better if the clients' code (if it turns out we can't have strafing) restricts movement, and calculates the appropriate changeVelocity and changeAngle calls.

  void SetNeighbour(int placement, EpollConnection* socketHandle);
  
  void GotServerRobot(ServerRobot message);

};

#endif
