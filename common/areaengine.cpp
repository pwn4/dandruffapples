#include "areaengine.h"
#include "except.h"
#include <algorithm>

using namespace std;

/*-----------======== Area Engine ========-----------*/
//NOTES:
//
//This engine assumes that every puck is of size 1.
//If you want smaller pucks, make bigger robots.
//Also, regionservers don't know who they are in the world.
//It makes things cleaner, easier, and more extendable
//for regionservers just to think of things in terms of
//their own region

Index AreaEngine::getRobotIndices(double x, double y){
  return Index((int)(x/regionBounds), (int)(y/regionBounds));
}

//constructor
AreaEngine::AreaEngine(int robotSize, int regionSize, int minElementSize) {
  //format of constructor call:
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //maximum number of bytes to allocation for a[][] elements (1GB?)

  robotRatio = robotSize;
  regionRatio = regionSize;
  
  //create our storage array with element size determined by our parameters
  //ensure regionSize can be split nicely
  if((regionSize/robotSize)*robotSize != regionSize)
  {
    throw SystemError("RegionSize not a multiple of RobotSize.");
  }
  if((regionSize/minElementSize)*minElementSize != regionSize)
  {
    throw SystemError("RegionSize not a multiple of minElementSize.");
  }
  
  //create the arrays
  puckArray = new int*[regionRatio];
  for(int i = 0; i < regionRatio; i++)
    puckArray[i] = new int[regionRatio];
  regionBounds = max(regionRatio/robotRatio, regionRatio/minElementSize) + 2; //add two for the overlaps in regions
  robotArray = new ArrayObject*[regionBounds];
  for(int i = 0; i < regionBounds; i++)
    robotArray[i] = new ArrayObject[regionBounds];
  
}

//destructor for cleanup
AreaEngine::~AreaEngine() {
  for(int i = 0; i < regionRatio; i++)
    delete[] puckArray[i];
  for(int i = 0; i < regionBounds; i++)
    delete[] robotArray[i];
  delete[] puckArray;
  delete[] robotArray;
}

//add a robot to the system. returns the robotobject that is created for convenience
RobotObject AreaEngine::AddRobot(double newx, double newy, double newvx, double newvy){
  //O(1) insertion
  Index robotIndices = getRobotIndices(newx, newy);
  RobotObject newRobot (newx, newy, newvx, newvy, robotIndices);
  
  //find where it belongs in a[][] and add it
  ArrayObject element = robotArray[robotIndices.x][robotIndices.y];
  //check if the area is empty first
  if(element.lastRobot == NULL)
  {
    element.robots = &(newRobot);
    element.lastRobot = &(newRobot);
  }else{
    element.lastRobot->nextRobot = &(newRobot);
    element.lastRobot = &(newRobot);
  }
  
  
  return newRobot;
}
