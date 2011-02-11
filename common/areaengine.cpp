#include "areaengine.h"
#include "except.h"
#include <algorithm>
#include <iostream>

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
  return Index((int)(x/elementSize), (int)(y/elementSize));
}

//constructor
AreaEngine::AreaEngine(int robotSize, int regionSize, int minElementSize) {
  //format of constructor call:
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //min a[][] element size in terms of pucks

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
  regionBounds = max(regionRatio/robotRatio, regionRatio/minElementSize); 
  elementSize = regionRatio/regionBounds;
  //add two for the overlaps in regions
  regionRatio += 2;
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
//overload
RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy){
  return AreaEngine::AddRobot(robotId, newx, newy, 0, 0);
}

RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy, double newvx, double newvy){
  //O(1) insertion
  Index robotIndices = getRobotIndices(newx, newy);
  RobotObject* newRobot = new RobotObject(robotId, newx, newy, newvx, newvy, robotIndices);
  
  //find where it belongs in a[][] and add it
  ArrayObject *element = &robotArray[robotIndices.x][robotIndices.y];
  //check if the area is empty first
  if(element->lastRobot == NULL)
  {
    element->robots = newRobot;
    element->lastRobot = newRobot;
  }else{
    element->lastRobot->nextRobot = newRobot;
    element->lastRobot = newRobot;
  }
  
  return newRobot;
}

//remove a robot with id robotId from the a[xInd][yInd] array element. cleanup. returns true if a robot was deleted
bool AreaEngine::RemoveRobot(int robotId, int xInd, int yInd){
  //O(1) Deletion. Would be O(m), but m (robots in area) is bounded by a constant, so actually O(1)

  ArrayObject element = robotArray[xInd][yInd];
  //check if the area is empty first
  if(element.robots == NULL)
    return false;

  RobotObject *curRobot = element.robots;
  //check it if its first
  if(curRobot->id == robotId)
  {
    element.robots = curRobot->nextRobot;
    delete curRobot;
    return true;
  }
  RobotObject * lastRobot = curRobot;
  curRobot = lastRobot->nextRobot;
  
  while(curRobot != NULL){
    if(curRobot->id == robotId){
      //we've found it. Stitch up the list and return
      lastRobot->nextRobot = curRobot->nextRobot;
      delete curRobot;
      return true;
    }
  }
  
  return false;
}

