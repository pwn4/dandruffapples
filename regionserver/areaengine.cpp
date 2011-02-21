#include "areaengine.h"

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

//getRobotIndices. Clip boolean dicatates whether we trim to valid values only
Index AreaEngine::getRobotIndices(double x, double y, bool clip){    
  Index rtn (x/elementSize, y/elementSize);
  
  if(clip){
    if(rtn.x < 0)
      rtn.x = 0;
    if(rtn.y < 0)
      rtn.y = 0;
      
    if(rtn.x >= regionBounds+2)
      rtn.x = regionBounds+1;
    if(rtn.y >= regionBounds)
      rtn.y = regionBounds+1;
  }
  
  return rtn;
}

//constructor
AreaEngine::AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle, double maximumSpeed, double maximumRotate) {
  //format of constructor call:
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //min a[][] element size in terms of pucks

  robotRatio = robotSize;
  regionRatio = regionSize;
  curStep = 0;
  viewDist = viewDistance;
  viewAng = viewAngle;
  maxSpeed = maximumSpeed;
  maxRotate = maximumRotate;
  
  //init our surface
  stepImage = cairo_image_surface_create (IMAGEFORMAT , IMAGEWIDTH, IMAGEHEIGHT);
  stepImageDrawer = cairo_create (stepImage);
  
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
  neighbours = new EpollConnection*[8];
  for(int i = 0; i < 8; i++)
    neighbours[i] = NULL;
  puckArray = new int*[regionRatio];
  for(int i = 0; i < regionRatio; i++)
    puckArray[i] = new int[regionRatio];
  regionBounds = min(regionRatio/robotRatio, regionRatio/minElementSize); 
  elementSize = regionRatio/regionBounds;
  //add two for the overlaps in regions
  robotArray = new ArrayObject*[regionBounds+2];
  for(int i = 0; i < regionBounds + 2; i++)
    robotArray[i] = new ArrayObject[regionBounds+2];
  
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

//this is the color mapping method: teams->Color components
ColorObject colorFromTeam(int teamId){
  return ColorObject(0.1, 0.1, 0.1);
}

//this method checks if a robot at (x1,y1) sees a robot at (x2,y2)
bool AreaEngine::Sees(double x1, double y1, double x2, double y2){
//assumes robots can see from any part of themselves
  if(sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)) <= (viewDist+robotRatio))
    return true;
  return false;
}

//this method checks if two robots at (x1,y1) and (x2,y2) are in collision
bool AreaEngine::Collides(double x1, double y1, double x2, double y2){
  if(sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)) <= robotRatio)
    return true;
  return false;
}

void AreaEngine::Step(bool generateImage){
  //O(n*c) for some nice, small constant. Can we improve this with a collision library? Let's benchmark and find out
  curStep++;
  //some worker variables
  Index topLeft, botRight;
  map<int, bool> *nowSaw;
  map<int, RobotObject*>::iterator robotIt;
  
  MessageType type;
	int len;
	const void *buffer;
  //FORCE all updates FROM neighbors to be entered before we start the step (necessary for synchronization)
  for(int i = 0; i < 8; i++)
  {
    if(neighbours[i] != NULL){
      while((*neighbours)->reader.doRead(&type, &len, &buffer))
      {
        switch (type) {
				  case MSG_SERVERROBOT: {
				    ServerRobot serverrobot;
				    serverrobot.ParseFromArray(buffer, len);
				    GotServerRobot(serverrobot);
				    break;
				  }

			    default:
					cerr << "Unexpected readable message from Region\n";
					break;
				}
      }
    }
  }

  //iterate through our region's robots and simulate them
  for(robotIt=robots.begin() ; robotIt != robots.end(); robotIt++)
  {
    //NOTE: addrobot changes should only be taken in at the end of a timestep, AFTER simulation.
  
    RobotObject * curRobot = (*robotIt).second;
    
    //if the robot is from the future, don't check it
    if(curRobot->lastStep == curStep)
      continue;
    
    //this is the big one. This is the O(N^2) terror. Thankfully, in the worst case (current implementation)
    //it runs (viewingdist*360degrees)/robotsize.
    //So it's really not all that bad. Benchmarks! Will improve later, too.
    
    //collisions first - just check, and zero velocities if they would have collided
    //calculate the bounds of the a[][] elements we need to check
    topLeft = getRobotIndices(curRobot->x-(maxSpeed+robotRatio/2), curRobot->y-(maxSpeed+robotRatio/2), true);
    botRight = getRobotIndices(curRobot->x+(maxSpeed+robotRatio/2), curRobot->y+(maxSpeed+robotRatio/2), true);

    for(int j = topLeft.x; j <= botRight.x; j++)
      for(int k = topLeft.y; k <= botRight.y; k++)
      {
        //we now have an a[][] element to check. Iterate through the robots in it
        ArrayObject * element = &robotArray[j][k];

        RobotObject *otherRobot = element->robots;
        //check'em
        while(otherRobot != NULL){    
          if(curRobot->id != otherRobot->id){
            if(otherRobot->lastStep != curStep){
              if(AreaEngine::Collides(curRobot->x+curRobot->vx, curRobot->y+curRobot->vy, otherRobot->x+otherRobot->vx, otherRobot->y+otherRobot->vy))
              {
                //they would have collided. Set their speeds to zero. Lock their speed by updating the current timestamp
                curRobot->vx = 0;
                curRobot->vy = 0;
                otherRobot->vx = 0;
                otherRobot->vy = 0;
                
                //here we will send messages to these robots and those watching that they've stopped
                //TODO:add this networking code
                
                //the lastCollision time_t variable is checked by setVelocity when it's called
                curRobot->lastCollision = time(NULL);
                otherRobot->lastCollision = curRobot->lastCollision;
                
              }
            }else{
              //we're dealing with a future robot (anomaly)... in theory this should never collide
              if(AreaEngine::Collides(curRobot->x+curRobot->vx, curRobot->y+curRobot->vy, otherRobot->x, otherRobot->y))
              {
                //they would have collided. Set their speeds to zero. Lock their speed by updating the current timestamp
                curRobot->vx = 0;
                curRobot->vy = 0;
                otherRobot->vx = 0;
                otherRobot->vy = 0;
                
                //here we will send messages to these robots and those watching that they've stopped
                //TODO:add this networking code
                
                //the lastCollision time_t variable is checked by setVelocity when it's called
                curRobot->lastCollision = time(NULL);
                otherRobot->lastCollision = curRobot->lastCollision;
                
              }
            }
          }
          otherRobot = otherRobot->nextRobot;
        }
      }
  }

  if(generateImage){
    //clear the image
    cairo_set_source_rgb (stepImageDrawer, 1, 1, 1);
    cairo_paint (stepImageDrawer); 
    
    //perhaps just for now, outline the region's boundaries. That way we can see them in the viewer
    cairo_rectangle (stepImageDrawer, 0, 0, IMAGEWIDTH, IMAGEHEIGHT);
    //set the color
    cairo_set_source_rgb(stepImageDrawer, .6, .6, .6);
    cairo_stroke (stepImageDrawer);
  }
  
  int drawX, drawY;
  
  //move the robots, now that we know they won't collide. Because we may remove a robot, we have to increment ourselves
  for(robotIt=robots.begin() ; robotIt != robots.end(); )
  {
    RobotObject * curRobot = (*robotIt).second;
    //if the robot is from the future, don't move it
    if(curRobot->lastStep == curStep)
    {
      robotIt++;
      continue;
    }
      
    curRobot->x += curRobot->vx;
    curRobot->y += curRobot->vy;
    curRobot->lastStep = curStep;
    
    if(generateImage){
      //repaint the robot
      drawX = ((curRobot->x-(regionRatio/regionBounds)) / regionRatio)*IMAGEWIDTH;
      drawY = ((curRobot->y-(regionRatio/regionBounds)) / regionRatio)*IMAGEHEIGHT;
      //drawX = ((curRobot->x) / (regionRatio+(2*(regionRatio/regionBounds))))*IMAGEWIDTH;
      //drawY = ((curRobot->y) / (regionRatio+(2*(regionRatio/regionBounds))))*IMAGEHEIGHT;
      //don't draw the overlaps
      if(drawX >= 0 && drawX < IMAGEWIDTH && drawY >= 0 && drawY < IMAGEHEIGHT)
      {
        cairo_rectangle (stepImageDrawer, drawX, drawY, 1, 1);
        //set the color - HARDCODE FOR NOW
        if(curRobot->robotColor == "red")
          cairo_set_source_rgb(stepImageDrawer, 1, 0, 0);
        else if(curRobot->robotColor == "green")
          cairo_set_source_rgb(stepImageDrawer, 0, .5, 0);
        else if(curRobot->robotColor == "blue")
          cairo_set_source_rgb(stepImageDrawer, 0, 0, 1);
        else if(curRobot->robotColor == "orange")
          cairo_set_source_rgb(stepImageDrawer, .7, .4, .103);
        else
          cairo_set_source_rgb(stepImageDrawer, .1, .1, .1);
        cairo_fill (stepImageDrawer);
      }
    }
    //check if the robot moves through a[][]
    Index oldIndices = curRobot->arrayLocation;
    Index newIndices = getRobotIndices(curRobot->x, curRobot->y, false);
    
    if(newIndices.x != oldIndices.x || newIndices.y != oldIndices.y)
    {
      //the robot moved, so...if we no longer track it
      if(newIndices.x < 0 || newIndices.y < 0 || newIndices.x >= regionBounds+2 || newIndices.y >= regionBounds+2)
      {
        AreaEngine::RemoveRobot(curRobot->id, oldIndices.x, oldIndices.y, true);
        robots.erase(robotIt++);
        continue;
      }else{
        AreaEngine::RemoveRobot(curRobot->id, oldIndices.x, oldIndices.y, false);
        curRobot->arrayLocation = newIndices;
        AreaEngine::AddRobot(curRobot);
        //check if we need to inform a neighbor that its entered an overlap - but ONLY if we just entered an OVERLAP!
        BroadcastRobot(curRobot, oldIndices, newIndices);
      }
    }else
      curRobot->arrayLocation = newIndices;
      
    robotIt++;
  }

  //check for sight. Theoretically runs in O(n^2)+O(n)+O(m). In reality, runs O((viewdist*360degrees/robotsize)*robotsinregion)+O(2*(viewdist*360degrees/robotsize))
  //THIS IS THE BOTTLENECK RIGHT NOW
  for(robotIt=robots.begin() ; robotIt != robots.end(); robotIt++)
  {
  
    RobotObject * curRobot = (*robotIt).second;
    nowSaw = curRobot->lastSeen;
    
    //may make this better. don't need to check full 360 degrees if we only see a cone
    topLeft = getRobotIndices(curRobot->x-viewDist, curRobot->y-viewDist, true);
    botRight = getRobotIndices(curRobot->x+viewDist, curRobot->y+viewDist, true);

    for(int j = topLeft.x; j <= botRight.x; j++)
      for(int k = topLeft.y; k <= botRight.y; k++)
        {
          //we have an a[][] element again. Iterate through the robots in it
          ArrayObject * element = &robotArray[j][k];

          RobotObject *otherRobot = element->robots;
          //check its elements
          while(otherRobot != NULL) {    
            if(curRobot->id != otherRobot->id && AreaEngine::Sees(curRobot->x, curRobot->y, otherRobot->x, otherRobot->y)){
              //instead of forming nowSeen and lastSeen, and then comparing. Do that shit on the FLY.
              //first, that which we hadn't seen but now do
              if(nowSaw->find(otherRobot->id) == nowSaw->end())
              {
                nowSaw->insert(pair<int, bool>(otherRobot->id, true));
                //TODO: add network code... send to curRobot that it now sees *setIterator
              }
              //curRobot->nowSeen->insert(pair<int, bool>(otherRobot->id, true));
            }else{
            //then, that which we did see, but now don't
              if(nowSaw->find(otherRobot->id) != nowSaw->end())
              {
                nowSaw->erase(otherRobot->id);
                //TODO: add network code... send to curRobot that it no longer sees *setIterator
              }
            }

            otherRobot = otherRobot->nextRobot;
          }
        }
  }
  
  //FORCE all updates to neighbors to occur before we finish the step (necessary for synchronization)
  for(int i = 0; i < 8; i++)
  {
    if(neighbours[i] != NULL){
      while((*neighbours)->queue.remaining() != 0)
        (*neighbours)->queue.doWrite();
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
  oldRobot->nextRobot = NULL;
}

RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy, double newa, double newvx, double newvy, int atStep, string newColor, bool broadcast){
  //O(1) insertion  
  Index robotIndices = getRobotIndices(newx, newy, true);
  
  RobotObject* newRobot = new RobotObject(robotId, newx, newy, newa, newvx, newvy, robotIndices, atStep, newColor);

  //add the robot to our robots vector (used for timestepping)
  robots.insert(pair<int, RobotObject*>(robotId, newRobot));
  
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
  newRobot->nextRobot = NULL;
  
  if(broadcast)
    BroadcastRobot(newRobot, Index(regionBounds/2, regionBounds/2), robotIndices);
  
  return newRobot;
}

//remove a robot with id robotId from the a[xInd][yInd] array element. cleanup. returns true if a robot was deleted
bool AreaEngine::RemoveRobot(int robotId, int xInd, int yInd, bool freeMem){
  //O(1) Deletion. Would be O(m), but m (robots in area) is bounded by a constant, so actually O(1)

  ArrayObject *element = &robotArray[xInd][yInd];
  //check if the area is empty first
  if(element->robots == NULL)
    return false;

  RobotObject *curRobot = element->robots;
  //check it if its first
  if(curRobot->id == robotId)
  {
    element->robots = curRobot->nextRobot;
    if(freeMem){
      delete curRobot;
    }
    return true;
  }

  RobotObject * lastRobot = curRobot;
  curRobot = lastRobot->nextRobot;
  
  while(curRobot != NULL){
    if(curRobot->id == robotId){
      //we've found it. Stitch up the list and return
      lastRobot->nextRobot = curRobot->nextRobot;
      if(freeMem){
        delete curRobot;
      }
      return true;
    }
    lastRobot = curRobot;
    curRobot = lastRobot->nextRobot;
  }
  
  return false;
}

bool AreaEngine::ChangeVelocity(int robotId, double newvx, double newvy){
  if(robots.find(robotId) == robots.end())
    return false;

  //enforce max speed
  double len = sqrt(newvx*newvx + newvy*newvy);
  if(len > maxSpeed)
  {
    newvx *= (maxSpeed/len);
    newvy *= (maxSpeed/len);
  }
  
  robots[robotId]->vx = newvx;
  robots[robotId]->vy = newvy;

  // Send updates to the clients.
  // TODO: Set seenByIDs
  // TODO: Set collision
  ServerRobot serverrobot;
  serverrobot.set_id(robotId);
  serverrobot.set_velocityx(newvx); 
  serverrobot.set_velocityy(newvy); 
  serverrobot.set_x(robots[robotId]->x); 
  serverrobot.set_y(robots[robotId]->y); 
  serverrobot.set_angle(robots[robotId]->angle); 
  serverrobot.set_haspuck(robots[robotId]->holdingPuck); 
  for (vector<EpollConnection*>::iterator it = controllers.begin();
       it != controllers.end(); it++) {
    (*it)->queue.push(MSG_SERVERROBOT, serverrobot);
    (*it)->set_writing(true); 
  }

  return true;
}

//NOTE: newangle is not the desired angle, it is the angle amount to rotate by!
bool AreaEngine::ChangeAngle(int robotId, double newangle){
  if(robots.find(robotId) == robots.end())
    return false;
  
  if(fabs(newangle) > maxRotate){
    if(newangle < 0)
      newangle = -1*maxRotate;
    else
      newangle = maxRotate;
  }

  robots[robotId]->angle += newangle;
  
  while(robots[robotId]->angle >= 2*M_PI)
    robots[robotId]->angle -= 2*M_PI;
    
  while(robots[robotId]->angle < 0)
    robots[robotId]->angle += 2*M_PI;;
     
  return true;
}

//tell the areaengine that a socket handle is open to write updates to
void AreaEngine::SetNeighbour(int placement, EpollConnection *socketHandle){
  neighbours[placement] = socketHandle;
}

void AreaEngine::GotServerRobot(ServerRobot message){
  if(robots.find(message.id()) == robots.end()){
    //new robot
    AddRobot(message.id(), message.x(), message.y(), message.angle(), message.velocityx(), message.velocityy(), curStep, message.color(), false);
  }else{
    //modify existing;
    cout << "FFFFFFUUU " << message.id() << endl;
    RobotObject* curRobot = robots[message.id()];
    
    if(message.has_velocityx()) 
      curRobot->vx = message.velocityx();
      
    if(message.has_velocityy()) 
      curRobot->vy = message.velocityy();
  }
}

void AreaEngine::AddController(EpollConnection *socketHandle){
  controllers.push_back(socketHandle);
}

void AreaEngine::BroadcastRobot(RobotObject *curRobot, Index oldIndices, Index newIndices){
  ServerRobot informNeighbour;
  informNeighbour.set_id(curRobot->id);
  //setup to translate the x and y
  int transx = curRobot->x;
  int transy = curRobot->y;
  //continue
  informNeighbour.set_color(curRobot->robotColor);
  informNeighbour.set_velocityx(curRobot->vx);
  informNeighbour.set_velocityy(curRobot->vy);
  informNeighbour.set_laststep(curStep);

  bool keepGoing = true;
  for (int i = 0; i < 8 && keepGoing; i++) {
    if (neighbours[i] == NULL) {
      // TODO: Check first if the robot will wrap to ourselves. 
      // Commented code below is for temporary reference.

      /*
      if(newIndices.x == 1){
        if(oldIndices.x > 1){
          curRobot->x = transx + regionRatio;
          keepGoing = false;
        }
        if(newIndices.y == 1 && neighbours[TOP_LEFT] != NULL){
          if(oldIndices.x > 1 || oldIndices.y > 1){
            if (neighbours[TOP_LEFT] != NULL) {
              informNeighbour.set_x(transx + regionRatio);
              informNeighbour.set_y(transy + regionRatio);
              neighbours[TOP_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
              neighbours[TOP_LEFT]->set_writing(true);
            } else {
              curRobot->x = transx + regionRatio;
              curRobot->y = transy + regionRatio;
            }
          }
        }else if(newIndices.y == regionBounds && neighbours[BOTTOM_LEFT] != NULL){
          if(oldIndices.x > 1 || oldIndices.y < regionBounds){
            if (neighbours[BOTTOM_LEFT] != NULL) {
              informNeighbour.set_x(transx + regionRatio);
              informNeighbour.set_y(transy - regionRatio);
              neighbours[BOTTOM_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
              neighbours[BOTTOM_LEFT]->set_writing(true);
            } else {
              curRobot->x = transx + regionRatio;
              curRobot->y = transy - regionRatio;
            }
          }
        }
      }else if(newIndices.x == regionBounds && neighbours[RIGHT] != NULL){
        if(oldIndices.x < regionBounds){
          if (neighbours[RIGHT] != NULL) {
            informNeighbour.set_x(transx - regionRatio);
            informNeighbour.set_y(transy);
            neighbours[RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
            neighbours[RIGHT]->set_writing(true);
          } else {
            curRobot->x = transx - regionRatio;
          }
        }
        if(newIndices.y == 1 && neighbours[TOP_RIGHT] != NULL){
          if(oldIndices.x < regionBounds || oldIndices.y > 1){
            if (neighbours[TOP_RIGHT] != NULL) {
              informNeighbour.set_x(transx - regionRatio);
              informNeighbour.set_y(transy + regionRatio);
              neighbours[TOP_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
              neighbours[TOP_RIGHT]->set_writing(true);
            } else {
              curRobot->x = transx - regionRatio;
              curRobot->y = transy + regionRatio;
            }
          }
        }else if(newIndices.y == regionBounds && neighbours[BOTTOM_RIGHT] != NULL){
          if(oldIndices.x < regionBounds || oldIndices.y < regionBounds){
            if (neighbours[BOTTOM_RIGHT] != NULL) {
              informNeighbour.set_x(transx - regionRatio);
              informNeighbour.set_y(transy - regionRatio);
              neighbours[BOTTOM_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
              neighbours[BOTTOM_RIGHT]->set_writing(true);
            } else {
              curRobot->x = transx - regionRatio;
              curRobot->y = transy - regionRatio;
            }
          }
        }
      }
      if(newIndices.y == 1 && neighbours[TOP] != NULL){
        if(oldIndices.y > 1){
          if (neighbours[TOP] != NULL) {
            informNeighbour.set_x(transx);
            informNeighbour.set_y(transy + regionRatio);
            neighbours[TOP]->queue.push(MSG_SERVERROBOT, informNeighbour);
            neighbours[TOP]->set_writing(true);
          } else {
            curRobot->y = transy + regionRatio;
          }
        }
      }else if(newIndices.y == regionBounds && neighbours[BOTTOM] != NULL){
        if(oldIndices.y < regionBounds){
          if (neighbours[BOTTOM] != NULL) {
            informNeighbour.set_x(transx);
            informNeighbour.set_y(transy - regionRatio);
            neighbours[BOTTOM]->queue.push(MSG_SERVERROBOT, informNeighbour);
            neighbours[BOTTOM]->set_writing(true);
          } else {
            curRobot->y = transy - regionRatio;
          }
        }
      }
    */   
    }
  } 

  if(newIndices.x == 1 && neighbours[LEFT] != NULL){
    if(oldIndices.x > 1){
      informNeighbour.set_x(transx + regionRatio);
      informNeighbour.set_y(transy);
      neighbours[LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
      neighbours[LEFT]->set_writing(true);
	  }
		if(newIndices.y == 1 && neighbours[TOP_LEFT] != NULL){
		  if(oldIndices.x > 1 || oldIndices.y > 1){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
        neighbours[TOP_LEFT]->set_writing(true);
	    }
    }else if(newIndices.y == regionBounds && neighbours[BOTTOM_LEFT] != NULL){
      if(oldIndices.x > 1 || oldIndices.y < regionBounds){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
        neighbours[BOTTOM_LEFT]->set_writing(true);
	    }
    }
  }else if(newIndices.x == regionBounds && neighbours[RIGHT] != NULL){
    if(oldIndices.x < regionBounds){
      informNeighbour.set_x(transx - regionRatio);
      informNeighbour.set_y(transy);
      neighbours[RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
      neighbours[RIGHT]->set_writing(true);
		}
		if(newIndices.y == 1 && neighbours[TOP_RIGHT] != NULL){
		  if(oldIndices.x < regionBounds || oldIndices.y > 1){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
        neighbours[TOP_RIGHT]->set_writing(true);
	    }
    }else if(newIndices.y == regionBounds && neighbours[BOTTOM_RIGHT] != NULL){
      if(oldIndices.x < regionBounds || oldIndices.y < regionBounds){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
        neighbours[BOTTOM_RIGHT]->set_writing(true);
	    }
    }
  }
  if(newIndices.y == 1 && neighbours[TOP] != NULL){
    if(oldIndices.y > 1){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy + regionRatio);
      neighbours[TOP]->queue.push(MSG_SERVERROBOT, informNeighbour);
      neighbours[TOP]->set_writing(true);
	  }
  }else if(newIndices.y == regionBounds && neighbours[BOTTOM] != NULL){
    if(oldIndices.y < regionBounds){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy - regionRatio);
      neighbours[BOTTOM]->queue.push(MSG_SERVERROBOT, informNeighbour);
      neighbours[BOTTOM]->set_writing(true);
    }
  }
}
