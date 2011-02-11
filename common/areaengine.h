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
  double x, y;
  double vx, vy;
  
  RobotObject * nextRobot;
  
  RobotObject(double newx, double newy) : x(newx), y(newy), vx(0), vy(0) {}
  RobotObject(double newx, double newy, double newvx, double newvy) : x(newx), y(newy), vx(newvx), vy(newvy) {}
};

struct ArrayObject{
  PuckStackObject * pucks;
  RobotObject * robots;
  
  ArrayObject() : pucks(NULL), robots(NULL) {}
};

class AreaEngine {
protected:
int robotRatio, regionRatio;    //robotDiameter:puckDiameter, regionSideLength:puckDiameter
int regionBounds;
int** puckArray;
ArrayObject** robotArray;
  
public:
  Index getRobotIndices(double x, double y);
  RobotObject AddRobot(double newx, double newy, double newvx, double newvy);
  AreaEngine(int robotSize, int regionSize, int minElementSize);
  ~AreaEngine();

};

#endif
