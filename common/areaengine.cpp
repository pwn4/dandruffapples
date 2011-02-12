#include "areaengine.h"
#include "except.h"
#include <algorithm>
#include <iostream>
#include <limits.h>
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include <math.h>

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
  return Index((int)(max(x, 0.0)/elementSize), (int)(max(y, 0.0)/elementSize));
}

//constructor
AreaEngine::AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle) {
  //format of constructor call:
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //min a[][] element size in terms of pucks

  robotRatio = robotSize;
  regionRatio = regionSize;
  curStep = 0;
  viewDist = viewDistance;
  viewAng = viewAngle;
  
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

//this method checks if two robots at (x1,y1) and (x2,y2) are in collision
bool AreaEngine::Collides(double x1, double y1, double x2, double y2){
  if(sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)) <= robotRatio)
    return true;
  return false;
}

void AreaEngine::Step(){
  //O(n*c) for some nice, small constant. Can we improve this with a collision library? Let's benchmark and find out
  curStep++;
  //iterate through our region's robots and simulate them
  for(int i = 0; i < robots.size(); i++)
  {
    //only check if its behind in the step. This fixes the case where a robot was just pushed from a neighbor
    //if(robots[i]->lastStep < curStep)
    //{
    //don't check this. addrobot should only be pushed at the end of a timestep.
    
      RobotObject * curRobot = robots[i];
      
      //this is the big one. This is the O(N^2) terror. Thankfully, in the worst case (current implementation)
      //it runs (viewingdist*360degrees)/robotsize.
      //So it's really not all that bad. Benchmarks! Will improve later, too.
      
      //collisions first - just check, and zero velocities if they would have collided
      //calculate the bounds of the a[][] elements we need to check
      Index topLeft = getRobotIndices(curRobot->x-robotRatio, curRobot->y-robotRatio);
      Index botRight = getRobotIndices(curRobot->x+robotRatio, curRobot->y+robotRatio);

      for(int j = topLeft.x; j <= botRight.x; j++)
        for(int k = topLeft.y; k <= botRight.y; k++)
        {
          //we now have an a[][] element to check. Iterate through the robots in it
          ArrayObject * element = &robotArray[j][k];

          RobotObject *otherRobot = element->robots;
          //check it if its first
          while(otherRobot != NULL){         
            if(curRobot->id != otherRobot->id && AreaEngine::Collides(curRobot->x+curRobot->vx, curRobot->y+curRobot->vy, otherRobot->x+otherRobot->vx, otherRobot->y+otherRobot->vy))
            {
              //they would have collided. Set their speeds to zero. Lock their speed by updating the current timestamp
              curRobot->vx = 0;
              curRobot->vy = 0;
              otherRobot->vx = 0;
              otherRobot->vy = 0;
              
              //comment out these two lines to prevent everything from stopping on collision
              cout << "WORLDS COLLIDE! (Ie two robots have collided)" << endl;
              //exit(1);
              
              //the lastCollision time_t variable is checked by setVelocity is called
              curRobot->lastCollision = time(NULL);
              otherRobot->lastCollision = curRobot->lastCollision;
              
            }
            otherRobot = otherRobot->nextRobot;
          }
        }
    //}
  }
  
  //here is where the sight code will go. LOL they're blind robots for now

  //move the robots, now that we know they won't collide
  for(int i = 0; i < robots.size(); i++)
  {
    RobotObject * curRobot = robots[i];
    curRobot->x += curRobot->vx;
    curRobot->y += curRobot->vy;
    //check if the robot moves through a[][]
    Index oldIndices = curRobot->arrayLocation;
    curRobot->arrayLocation = getRobotIndices(curRobot->x, curRobot->y);
    if(curRobot->arrayLocation.x != oldIndices.x || curRobot->arrayLocation.y != oldIndices.y)
    {
      //the robot moved, so...
      /*cout << "MOVING " << curRobot->id << " TO (" << curRobot->arrayLocation.x << ", " << 
      curRobot->arrayLocation.y << ") FROM (" << oldIndices.x << ", " << oldIndices.y << ")" << endl;*/
      AreaEngine::AddRobot(curRobot);
      AreaEngine::RemoveRobot(curRobot->id, oldIndices.x, oldIndices.y, false);
    }
  }
}

//add a robot to the system. returns the robotobject that is created for convenience
//overload
void AreaEngine::AddRobot(RobotObject * oldRobot){
  
  //find where it belongs in a[][] and add it
  ArrayObject *element = &robotArray[oldRobot->arrayLocation.x][oldRobot->arrayLocation.y];
  //check if the area is empty first
  if(element->lastRobot == NULL)
  {
    element->robots = oldRobot;
    element->lastRobot = oldRobot;
  }else{
    element->lastRobot->nextRobot = oldRobot;
    element->lastRobot = oldRobot;
  }

}

RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy, int atStep){
  return AreaEngine::AddRobot(robotId, newx, newy, 0, 0, atStep);
}

RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy, double newvx, double newvy, int atStep){
  //O(1) insertion
  Index robotIndices = getRobotIndices(newx, newy);
  RobotObject* newRobot = new RobotObject(robotId, newx, newy, newvx, newvy, robotIndices, atStep);

  //add the robot to our robots vector (used for timestepping)
  robots.push_back(newRobot);
  
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
bool AreaEngine::RemoveRobot(int robotId, int xInd, int yInd, bool freeMem){
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
    if(freeMem)
      delete curRobot;
    return true;
  }
  RobotObject * lastRobot = curRobot;
  curRobot = lastRobot->nextRobot;
  
  while(curRobot != NULL){
    if(curRobot->id == robotId){
      //we've found it. Stitch up the list and return
      lastRobot->nextRobot = curRobot->nextRobot;
      if(freeMem)
        delete curRobot;
      return true;
    }
  }
  
  return false;
}

