#ifndef _AREAENGINE_H_
#define _AREAENGINE_H_

#include <cstddef>


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
  Index arrayLocation;
  int id;
  double x, y;
  double vx, vy;
  
  RobotObject * nextRobot;
  
  RobotObject(int newid, double newx, double newy, Index aLoc) : id(newid), x(newx), y(newy), vx(0), vy(0) {arrayLocation = aLoc;}
  RobotObject(int newid, double newx, double newy, double newvx, double newvy, Index aLoc) : id(newid), x(newx), y(newy), vx(newvx), vy(newvy) {arrayLocation = aLoc;}
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
int** puckArray;
ArrayObject** robotArray;
  
public:
  Index getRobotIndices(double x, double y);
  RobotObject* AddRobot(int robotId, double newx, double newy, double newvx, double newvy);
  bool RemoveRobot(int robotId, int xInd, int yInd);
  AreaEngine(int robotSize, int regionSize, int minElementSize);
  ~AreaEngine();

};

#endif
