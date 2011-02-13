#ifndef _AREAENGINE_H_
#define _AREAENGINE_H_

#include <cstddef>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <set>
#include <map>

using namespace std;

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
  double vx, vy;
  bool holdingPuck;
  Index arrayLocation;
  time_t lastCollision;
  map<int, bool> *lastSeen;
  map<int, bool> *nowSeen; //used for sight calculations
  
  RobotObject * nextRobot;
  
  RobotObject(int newid, double newx, double newy, Index aLoc, int curStep) : id(newid), lastStep(curStep), x(newx), y(newy), vx(0), vy(0), arrayLocation(aLoc), lastCollision(time(NULL)), lastSeen(new map<int, bool>), nowSeen(new map<int, bool>), nextRobot(NULL) {}
  RobotObject(int newid, double newx, double newy, double newvx, double newvy, Index aLoc, int curStep) : id(newid), lastStep(curStep), x(newx), y(newy), vx(newvx), vy(newvy), arrayLocation(aLoc), lastCollision(time(NULL)), lastSeen(new map<int, bool>), nowSeen(new map<int, bool>), nextRobot(NULL) {}
};

struct ArrayObject{
  PuckStackObject * pucks;
  RobotObject * robots;
  
  RobotObject * lastRobot;
  PuckStackObject * lastPuckStack;
  
  ArrayObject() : pucks(NULL), robots(NULL), lastRobot(NULL), lastPuckStack(NULL) {}
};

class AreaEngine {
protected:
int robotRatio, regionRatio;    //robotDiameter:puckDiameter, regionSideLength:puckDiameter
int regionBounds;
int elementSize;  //in pucks
double viewDist, viewAng;
int coolDown; //cooldown before being able to change velocities
int** puckArray;
double maxSpeed; //a bound on the speed of robots. Should be passed by the clock.
ArrayObject** robotArray;
vector<RobotObject*> robots;
  
public:
  int curStep;
  
  Index getRobotIndices(double x, double y);
  
  void Step();
  
  bool Sees(double x1, double y1, double x2, double y2);
  
  bool Collides(double x1, double y1, double x2, double y2);
  
  void AddRobot(RobotObject * oldRobot);
  RobotObject* AddRobot(int robotId, double newx, double newy, int atStep);
  RobotObject* AddRobot(int robotId, double newx, double newy, double newvx, double newvy, int atStep);
  
  bool RemoveRobot(int robotId, int xInd, int yInd, bool freeMem);
  
  AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle, double maxSpeed);
  ~AreaEngine();

};

#endif
