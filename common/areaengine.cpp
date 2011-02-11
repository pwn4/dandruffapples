#include "areaengine.h"
#include "except.h"
#include <algorithm>

using namespace std;

AreaEngine::AreaEngine(int xdisp, int ydisp, int robotSize, int regionSize, int maxMemory) {
  //format of constructor call:
  //x and y displacement of region (assume topleft = 0,0)
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //maximum number of bytes to allocation for a[][] elements (1GB?)
  
  topx = xdisp;
  topy = ydisp;
  robotRatio = robotSize;
  regionRatio = regionSize;
  
  //create our storage array with element size determined by our parameters
  //ensure regionSize can be split nicely
  if((regionSize/robotSize)*robotSize != regionSize)
  {
    throw SystemError("RegionSize not a multiple of RobotSize.");
  }
  
  //create the arrays
  puckArray = new int*[regionRatio];
  for(int i = 0; i < regionRatio; i++)
    puckArray[i] = new int[regionRatio];
  int regionBounds = max(regionRatio/robotRatio, robotRatio);
  robotArray = new int*[regionBounds];
  for(int i = 0; i < regionBounds; i++)
    robotArray[i] = new int[regionBounds];
  
  
  
}

AreaEngine::~AreaEngine() {

}

