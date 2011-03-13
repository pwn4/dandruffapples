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
    if(rtn.y >= regionBounds+2)
      rtn.y = regionBounds+1;
  }
  
  if(rtn.x < -1 || rtn.y < -1 || rtn.x > regionBounds+2 || rtn.y > regionBounds+2)
    throw runtime_error("AreaEngine: Robot tracked too far out of bounds.");
  
  return rtn;
}

//constructor
AreaEngine::AreaEngine(int robotSize, int regionSize, int minElementSize, double viewDistance, double viewAngle, double maximumSpeed, double maximumRotate) {
  //format of constructor call:
  //robotDiameter:puckDiameter, regionSideLength:puckDiameter
  //min a[][] element size in terms of pucks

  robotRatio = robotSize;
  pickupRange = 4*(robotRatio / 2); //hard coded
  regionRatio = regionSize;
  curStep = 0;
  viewDist = viewDistance;
  viewAng = viewAngle;
  maxSpeed = maximumSpeed;
  maxRotate = maximumRotate;
  render.set_timestep(curStep);
  sightSquare = (viewDist+robotRatio)*(viewDist+robotRatio);
  collisionSquare = robotRatio*robotRatio;

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
  regionBounds = min(regionRatio/robotRatio, regionRatio/minElementSize); 
  elementSize = regionRatio/regionBounds;
  //add two for the overlaps in regions
  robotArray = new ArrayObject*[regionBounds+2];
  for(int i = 0; i < regionBounds + 2; i++)
    robotArray[i] = new ArrayObject[regionBounds+2];
  
}

//destructor for cleanup
AreaEngine::~AreaEngine() {
  for(int i = 0; i < regionBounds; i++)
    delete[] robotArray[i];
  delete[] robotArray;
}

//comparator for robot rendering
class CompareRobotObject {
    public:
    bool operator()(RobotObject*& r1, RobotObject*& r2) // Returns true if t1 is later than t2
    {
       if (r1->y > r2->y) return true;
       
       return false;
    }
};

//comparator for puck rendering
//its weird. > sorts ascending for robots, but descending for pucks... it's very weird.
bool ComparePuckStackObject::operator()(PuckStackObject* const &r1, PuckStackObject* const &r2) // Returns true if t1 is later than t2
{
   if (r1->y < r2->y) return true;
   
   return false;
}

//comparator for Command queueing
bool CompareCommand::operator()(Command* const &r1, Command* const &r2) // Returns true if t1 is later than t2
{
   if (r1->step > r2->step) return true;
   
   return false;
}

//this method checks if a robot at (x1,y1) sees a robot at (x2,y2)
bool AreaEngine::Sees(double x1, double y1, double x2, double y2){
//assumes robots can see from any part of themselves
  if((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) <= sightSquare)
    return true;
  return false;
}

//this method checks if two robots at (x1,y1) and (x2,y2) are in collision
bool AreaEngine::Collides(double x1, double y1, double x2, double y2){
  if((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) <= collisionSquare)
    return true;
  return false;
}

void AreaEngine::Step(bool generateImage){

  //new step
  curStep++;
  
  //some worker variables
  Index topLeft, botRight;
  map<int, RobotObject*>::iterator robotIt;
  map<int, bool> *nowSeenBy;
  map<PuckStackObject*, bool> *pucksNowSeen;
  
  if(curStep % 2 == 1)
  {
    //collision step
    
    forceUpdates();
    
    //apply all changes for this step before we simulate (clients)
    //we can allow 2 step old client messages, but not server ones
    while(clientChangeQueue.size() > 0)
    {
      Command *newCommand = clientChangeQueue.top();
      
      if(newCommand->step == curStep-1 || newCommand->step == curStep-2){
        
        // Send update over the network.
        ServerRobot serverrobot;
        serverrobot.set_id(newCommand->robotId);
        RobotObject* curRobot = robots[newCommand->robotId];
        
        if(newCommand->velocityx != INT_MAX) 
        {
          curRobot->vx = newCommand->velocityx;
          serverrobot.set_velocityx(curRobot->vx); 
        }
          
        if(newCommand->velocityy != INT_MAX)
        {
          curRobot->vy = newCommand->velocityy;
          serverrobot.set_velocityy(curRobot->vy); 
        }
          
        if(newCommand->angle != INT_MAX)
        {
          curRobot->angle = newCommand->angle;
          serverrobot.set_angle(curRobot->angle);
        }
          
        serverrobot.set_laststep(-1); //for stray packet detection
        //serverrobot.set_haspuck(robots[robotId]->holdingPuck); 

        // Inform robots that can see this one of state change.
        map<int, bool> *nowSeenBy = &robots[newCommand->robotId]->lastSeenBy;
        if ((*nowSeenBy).size() > 0) {
          map<int, bool>::iterator sightCheck;
          map<int, bool>::iterator sightEnd = (*nowSeenBy).end();
          for(sightCheck = (*nowSeenBy).begin(); sightCheck != sightEnd;
              sightCheck++)
          {
            SeesServerRobot* seesServerRobot = serverrobot.add_seesserverrobot();
            seesServerRobot->set_viewlostid(false); 
            seesServerRobot->set_seenbyid(sightCheck->first);
          }
        } 

        // Send updates to controllers.
        for (vector<EpollConnection*>::const_iterator it = controllers.begin();
             it != controllers.end(); it++) {
          (*it)->queue.push(MSG_SERVERROBOT, serverrobot);
          (*it)->set_writing(true); 
        }

        // Broadcast velocity change to other servers - BroadcastRobot checks if robot is in border cell
        BroadcastRobot(robots[newCommand->robotId], Index(regionBounds/2, regionBounds/2), robots[newCommand->robotId]->arrayLocation, curStep);
        
        delete newCommand;
        clientChangeQueue.pop();
      }else if(newCommand->step < curStep-2)
        throw SystemError("AreaEngine: Old client robot change leftover in collision.");
      else
        break;
    }
    
    //apply all changes for this step before we simulate (servers)
    while(serverChangeQueue.size() > 0)
    {
      Command *newCommand = serverChangeQueue.top();
      
      if(newCommand->step == curStep-1){

       //TODO: dirty fix for a seg fault. FIX ME ASAP
       if( robots.find(newCommand->robotId) == robots.end())
       {
           //throw runtime_error("1");
           delete newCommand;
           serverChangeQueue.pop();
          continue;
       }

        RobotObject* curRobot = robots[newCommand->robotId];
        
        if(newCommand->velocityx != INT_MAX) 
          curRobot->vx = newCommand->velocityx;
          
        if(newCommand->velocityy != INT_MAX)
          curRobot->vy = newCommand->velocityy;
          
        if(newCommand->angle != INT_MAX)
          curRobot->angle = newCommand->angle;
          
        delete newCommand;
        serverChangeQueue.pop();
      }else if(newCommand->step < curStep-1)
        throw SystemError("AreaEngine: Old server robot change leftover in collision.");
      else
        break;
    }
    
    set<RobotObject*> changedRobots;
    
    //iterate through our region's robots and simulate them
    for(robotIt=robots.begin() ; robotIt != robots.end(); robotIt++)
    {
      //NOTE: addrobot changes should only be taken in at the end of a timestep, AFTER simulation.
    
        //TODO: dirty fix for a seg fault #2. FIX ME ASAP
        if( (*robotIt).second == NULL)
        {
          throw runtime_error("2");
        	robots.erase(robotIt++);
           continue;
        }

      RobotObject * curRobot = (*robotIt).second;
      
      //if the robot is from the future, we should now throw an error because we are now super synchronized
      if(curRobot->lastStep >= curStep)
        throw SystemError("AreaEngine: Robot time travel occurred during collision.");
        
      //bring it into the present
      curRobot->lastStep = curStep;
      
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
              if(AreaEngine::Collides(curRobot->x+curRobot->vx, curRobot->y+curRobot->vy, otherRobot->x+otherRobot->vx, otherRobot->y+otherRobot->vy))
              {
                //note velocity changes
                if(curRobot->vx != 0 || curRobot->vy != 0)
                  changedRobots.insert(curRobot); // send update over the network
                if(otherRobot->vx != 0 || otherRobot->vy != 0)
                  changedRobots.insert(otherRobot); // send update over the network
                
                //they would have collided. Set their speeds to zero. Lock their speed by updating the current timestamp
                curRobot->vx = 0;
                curRobot->vy = 0;
                otherRobot->vx = 0;
                otherRobot->vy = 0;
                
                //the lastCollision time_t variable is checked by setVelocity when it's called
                curRobot->lastCollision = time(NULL);
                otherRobot->lastCollision = curRobot->lastCollision;
                
                //inform
                BroadcastRobot(curRobot, Index(regionBounds/2, regionBounds/2), curRobot->arrayLocation, curStep);
                BroadcastRobot(otherRobot, Index(regionBounds/2, regionBounds/2), otherRobot->arrayLocation, curStep);
              }
            }
            otherRobot = otherRobot->nextRobot;
          }
        }
    }
      
    set<RobotObject*>::iterator changedIt;
    set<RobotObject*>::iterator endChangedIt = changedRobots.end();

    for(changedIt = changedRobots.begin(); changedIt != endChangedIt; changedIt++)
    { 
      RobotObject * curRobot = *changedIt;
    
      ServerRobot serverrobot;
      serverrobot.set_id(curRobot->id);
      serverrobot.set_laststep(-1); //set this to -1 so that we can detect stray messages
      serverrobot.set_velocityx(curRobot->vx);
      serverrobot.set_velocityy(curRobot->vy); 
      serverrobot.set_angle(curRobot->angle);
      serverrobot.set_hascollided(true); 
      serverrobot.set_haspuck(false); 

      // Is this robot seen by others?
      nowSeenBy = &(curRobot->lastSeenBy);
      if ((*nowSeenBy).size() > 0) {
        map<int, bool>::iterator sightCheck;
        map<int, bool>::iterator sightEnd = (*nowSeenBy).end();
        for(sightCheck = (*nowSeenBy).begin(); sightCheck != sightEnd;
            sightCheck++)
        {
          SeesServerRobot* seesServerRobot = serverrobot.add_seesserverrobot();
          seesServerRobot->set_viewlostid(false); 
          seesServerRobot->set_seenbyid(sightCheck->first);
          // Omitting relx and rely data. Updates will come in the
          // sight loop.
        }
      }else //if not, send nothing
        continue;

      // Send update.
      for (vector<EpollConnection*>::const_iterator it = 
           controllers.begin(); it != controllers.end(); it++) {
        (*it)->queue.push(MSG_SERVERROBOT, serverrobot);
        (*it)->set_writing(true); 
      }
    }
    
  }else{
    //simulation step
    
    forceUpdates();
    
    //apply all changes for this step before we simulate (servers)
    while(serverChangeQueue.size() > 0)
    {
      Command *newCommand = serverChangeQueue.top();
      
      if(newCommand->step == curStep-1){
        
        RobotObject* curRobot = robots[newCommand->robotId];
        
        if(newCommand->velocityx != INT_MAX) 
          curRobot->vx = newCommand->velocityx;
          
        if(newCommand->velocityy != INT_MAX)
          curRobot->vy = newCommand->velocityy;
          
        if(newCommand->angle != INT_MAX)
          curRobot->angle = newCommand->angle;
          
        delete newCommand;
        serverChangeQueue.pop();
      }else if(newCommand->step < curStep-1)
        throw SystemError("AreaEngine: Old server robot change leftover in sim.");
      else
        break;
    }

    //prepare the priority queue
    priority_queue<RobotObject*, vector<RobotObject*>, CompareRobotObject> pq;
    
    //move the robots, now that we know they won't collide. Because we may remove a robot, we have to increment ourselves
    for(robotIt=robots.begin() ; robotIt != robots.end(); )
    {
        //TODO: dirty fix for a seg fault #3. FIX ME ASAP
        if( (*robotIt).second == NULL)
        {
        throw runtime_error("3");
        	robots.erase(robotIt++);
           continue;
        }

      RobotObject * curRobot = (*robotIt).second;
      //if the robot is from the future, don't move it - can happen with fast vs slow neighbors
      if(curRobot->lastStep == curStep)
      {
        robotIt++;
        continue;
      }
      if(curRobot->lastStep > curStep)
        throw SystemError("AreaEngine: Far future robot in sim.");
        
      curRobot->x += curRobot->vx;
      curRobot->y += curRobot->vy;
      curRobot->lastStep = curStep;
      
      //check if the robot moves through a[][]
      Index oldIndices = curRobot->arrayLocation;
      Index newIndices = getRobotIndices(curRobot->x, curRobot->y, false);

      if(newIndices.x != oldIndices.x || newIndices.y != oldIndices.y)
      {
        //did the robot move such that we now own it? Making this a separate
        //if statement for now incase I screw something up.
        if(!(newIndices.x < 1 || newIndices.y < 1 || newIndices.x > regionBounds 
            || newIndices.y > regionBounds) && (oldIndices.x < 1 
            || oldIndices.y < 1 || oldIndices.x > regionBounds
            || oldIndices.y > regionBounds)) {
          // All newIndices must be within our own, non-shared area.
          // And At least one oldIndices must be within the shared area.
          Claim claim;
          claim.set_id(curRobot->id);
          for (vector<EpollConnection*>::const_iterator it = controllers.begin();
               it != controllers.end(); it++) {
            (*it)->queue.push(MSG_CLAIM, claim);
            (*it)->set_writing(true); 
          }
        }

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
          BroadcastRobot(curRobot, oldIndices, newIndices, curStep);
        }
      }

      //add the robot to the sorted draw list
      pq.push(curRobot);

      robotIt++;
    }
    
    //do drawing
    if(generateImage){
      int drawX, drawY;
      int pdrawX, pdrawY;
      int minY = 0;
      render.clear_image();
      render.set_timestep(curStep);
      int curY = 0;
      
      map<PuckStackObject*, bool>::iterator puckIt;
      map<PuckStackObject*, bool>::iterator puckEnd = puckq.end();
      
      puckIt=puckq.begin();
      RobotObject* paintRobot = NULL;
      if(!pq.empty())
        paintRobot = pq.top();
      PuckStackObject* paintPuck = NULL;
      if(puckIt != puckEnd)
        paintPuck = (*puckIt).first;

      //we have to iterate through robots and pucks at the same time... it's TRICKAY! TRICKY TRICKY TRICKY TRICKY
      while(true)
      {
        if(pq.empty()){
          paintRobot = NULL;
          drawX = INT_MAX;
          drawY = INT_MAX;
        }else{
          drawX = ((paintRobot->x-(regionRatio/regionBounds)) / regionRatio)*IMAGEWIDTH;
          drawY = ((paintRobot->y-(regionRatio/regionBounds)) / regionRatio)*IMAGEHEIGHT;
        }
        
        if(puckIt == puckEnd){
          paintPuck = NULL;
          pdrawX = INT_MAX;
          pdrawY = INT_MAX;
        }else{
          pdrawX = (((double)(paintPuck->x)-(regionRatio/regionBounds)) / regionRatio)*IMAGEWIDTH;
          pdrawY = (((double)(paintPuck->y)-(regionRatio/regionBounds)) / regionRatio)*IMAGEHEIGHT;
        }
        
        if(paintRobot == NULL && paintPuck == NULL)
          break;
        
        //catch up with our Y's
        minY = min(drawY, pdrawY);
        
        while(curY < minY){
          render.add_image(BytePack(65535, 65535));
          curY++;
          
          //don't draw more than we have to
          if(curY > IMAGEHEIGHT)
            break;
        }
        
        if(curY > IMAGEHEIGHT)
          break;

        //don't draw the overlaps
        if(paintRobot != NULL && drawY == minY)
        {
          if(drawX >= 0 && drawX < IMAGEWIDTH && drawY >= 0 && drawY < IMAGEHEIGHT)
          {
            if(drawX >= 65535 || drawY >= 65535)
              throw SystemError("AreaEngine: Draw Size requested too large");

            render.add_image(BytePack(drawX, paintRobot->team));
          }
          pq.pop();
          paintRobot = pq.top();
        }
        
        //don't draw the overlaps
        if(paintPuck != NULL && pdrawY == minY)
        {
          if(pdrawX >= 0 && pdrawX < IMAGEWIDTH && pdrawY >= 0 && pdrawY < IMAGEHEIGHT)
          {
            if(pdrawX >= 65535 || pdrawY >= 65535)
              throw SystemError("AreaEngine: Draw Size requested too large");

            render.add_image(BytePack(pdrawX, 65534));  //let 65534 be reserved for pucks
            puckIt++;
            paintPuck = (*puckIt).first;
          }
        }
      }
    }
    
    //this map gets populated as we iterate through the robots - NICE! Multitasking!
    map<PuckStackObject*, PuckStack*> puckUpdates;
    
    //check for robot sight and puck sight at the same time.
    for(robotIt=robots.begin() ; robotIt != robots.end(); robotIt++)
    {
        //TODO: dirty fix for a seg fault #4. FIX ME ASAP
        if( (*robotIt).second == NULL)
        {
        throw runtime_error("4");
        	robots.erase(robotIt++);
           continue;
        }

      RobotObject * curRobot = (*robotIt).second;
      nowSeenBy = &(curRobot->lastSeenBy);  //for robot sight
      pucksNowSeen = &(curRobot->pucksSeen);  //for puck sight
      
      //may make this better. don't need to check full 360 degrees if we only see a cone
      topLeft = getRobotIndices(curRobot->x-viewDist, curRobot->y-viewDist, true);
      botRight = getRobotIndices(curRobot->x+viewDist, curRobot->y+viewDist, true);

      //create the serverrobot object
      ServerRobot serverrobot;
      serverrobot.set_id(curRobot->id);
      serverrobot.set_laststep(-1);   //allow stray msg detection
      
      //first check sight losses
      //NOTICE: We should not need to send sight losses. We still need to CALCULATE them, but we don't need to send them.
      //I am leaving the network code for it here for a little while until the client code to drop far robots it written
      map<int, bool>::iterator sightCheck;
      map<int, bool>::iterator sightEnd = (*nowSeenBy).end();
      map<PuckStackObject*, bool>::iterator pSightCheck;
      map<PuckStackObject*, bool>::iterator pSightEnd = (*pucksNowSeen).end();
      RobotObject *otherRobot;
      PuckStackObject *puckStack;
      
      //robot sight losses
      for(sightCheck = (*nowSeenBy).begin(); sightCheck != sightEnd; )
      {
        if(robots.find((*sightCheck).first) == robots.end())
        {
          nowSeenBy->erase(sightCheck++);
          continue;
        }
        otherRobot = robots[(*sightCheck).first];
        if(!AreaEngine::Sees(otherRobot->x, otherRobot->y, curRobot->x, curRobot->y)){
        
          nowSeenBy->erase(sightCheck++);
          
          //BEGIN DEPRICATED SECTION
          SeesServerRobot* seesServerRobot = serverrobot.add_seesserverrobot();
          seesServerRobot->set_viewlostid(true);
          seesServerRobot->set_seenbyid(otherRobot->id);
          seesServerRobot->set_relx(curRobot->x - otherRobot->x);
          seesServerRobot->set_rely(curRobot->y - otherRobot->y);
          //END OF DEPRICATED SECTION
          continue;
        }
        
        sightCheck++;
      }
      
      //puck sight losses
      for(pSightCheck = (*pucksNowSeen).begin(); pSightCheck != pSightEnd; )
      {
        PuckStack * puckStackAccess;
        puckStack = (*pSightCheck).first;
        if(puckq.find(puckStack) == puckq.end())
        {
          //check if this is the first seen by for the stack
          if(puckUpdates.find(puckStack) == puckUpdates.end()){
            PuckStack *newStack = new PuckStack;
            newStack->set_stacksize(puckStack->count);
            puckUpdates.insert(pair<PuckStackObject*, PuckStack*>(puckStack, newStack));
          }
        
          puckStackAccess = puckUpdates[puckStack];
          SeesPuckStack* seesPuckStack = puckStackAccess->add_seespuckstack();
          seesPuckStack->set_viewlostid(true);
          seesPuckStack->set_seenbyid(curRobot->id);
          seesPuckStack->set_relx(puckStack->x - curRobot->x);
          seesPuckStack->set_rely(puckStack->x - curRobot->y);
                    
          pucksNowSeen->erase(pSightCheck++);
          continue;
        }
        
        if(!AreaEngine::Sees(curRobot->x, curRobot->y, puckStack->x, puckStack->y)){
          //check if this is the first seen by for the stack
          if(puckUpdates.find(puckStack) == puckUpdates.end()){
            PuckStack *newStack = new PuckStack;
            newStack->set_stacksize(puckStack->count);
            puckUpdates.insert(pair<PuckStackObject*, PuckStack*>(puckStack, newStack));
          }
          
          puckStackAccess = puckUpdates[puckStack];
          SeesPuckStack* seesPuckStack = puckStackAccess->add_seespuckstack();
          seesPuckStack->set_viewlostid(true);
          seesPuckStack->set_seenbyid(curRobot->id);
          seesPuckStack->set_relx(puckStack->x - curRobot->x);
          seesPuckStack->set_rely(puckStack->x - curRobot->y);
        
          pucksNowSeen->erase(pSightCheck++);
          continue;
        }
        
        pSightCheck++;
      }

      //do puck and robot sight acquires
      for(int j = topLeft.x; j <= botRight.x; j++)
        for(int k = topLeft.y; k <= botRight.y; k++)
          {
            //we have an a[][] element again.
            ArrayObject * element = &robotArray[j][k];

            //do robots
            otherRobot = element->robots;

            while(otherRobot != NULL) {    
              if(curRobot->id != otherRobot->id){
                if(nowSeenBy->find(otherRobot->id) == nowSeenBy->end())
                {
                  if(AreaEngine::Sees(otherRobot->x, otherRobot->y, curRobot->x, curRobot->y)){
                  //instead of forming nowSeen and lastSeen, and then comparing. Do that shit on the FLY.
                  //first, that which we hadn't been seen by but now are

                    nowSeenBy->insert(pair<int, bool>(otherRobot->id, true));
                    SeesServerRobot* seesServerRobot = serverrobot.add_seesserverrobot();
                    seesServerRobot->set_viewlostid(false);
                    seesServerRobot->set_seenbyid(otherRobot->id);
                    seesServerRobot->set_relx(curRobot->x - otherRobot->x);
                    seesServerRobot->set_rely(curRobot->y - otherRobot->y);
                  }
                }
              }

              otherRobot = otherRobot->nextRobot;
            }
            
            //do pucks
            puckStack = element->pucks;
            PuckStack * puckStackAccess;

            while(puckStack != NULL) {    
              if(pucksNowSeen->find(puckStack) == pucksNowSeen->end())
              {
                if(AreaEngine::Sees(curRobot->x, curRobot->y, puckStack->x, puckStack->y)){
                  //we now see it
                  
                  //check if this is the first seen by for the stack
                  if(puckUpdates.find(puckStack) == puckUpdates.end()){
                    PuckStack *newStack = new PuckStack;
                    newStack->set_stacksize(puckStack->count);
                    puckUpdates.insert(pair<PuckStackObject*, PuckStack*>(puckStack, newStack));
                  }
                  
                  //note the info
                  pucksNowSeen->insert(pair<PuckStackObject*, bool>(puckStack, true));
                  puckStackAccess = puckUpdates[puckStack];
                  SeesPuckStack* seesPuckStack = puckStackAccess->add_seespuckstack();
                  seesPuckStack->set_viewlostid(false);
                  seesPuckStack->set_seenbyid(curRobot->id);
                  seesPuckStack->set_relx(puckStack->x - curRobot->x);
                  seesPuckStack->set_rely(puckStack->x - curRobot->y);
                }
              }

              puckStack = puckStack->nextStack;
            }
          }
          
      // Send update if at least one seenById exists
      if (serverrobot.seesserverrobot_size() > 0) {
        for (vector<EpollConnection*>::const_iterator it = 
             controllers.begin(); it != controllers.end(); it++) {
          (*it)->queue.push(MSG_SERVERROBOT, serverrobot);
          (*it)->set_writing(true); 
        }
      }
    }
    
    //send all the puck updates
    map<PuckStackObject*, PuckStack*>::iterator puckIt;
    for(puckIt=puckUpdates.begin() ; puckIt != puckUpdates.end(); puckIt++)
    {
      for (vector<EpollConnection*>::const_iterator it = 
           controllers.begin(); it != controllers.end(); it++) {
        (*it)->queue.push(MSG_PUCKSTACK, *((*puckIt).second));
        (*it)->set_writing(true); 
      }
      delete puckIt->second;
    }
    
  }
  
  flushNeighbours();
  
  flushControllers();

  //ZOMFG we're done

}

void AreaEngine::flushNeighbours(){
  //FORCE all updates to neighbors to occur before we finish the step (necessary for synchronization)
  for(int i = 0; i < 8; i++)
  {
    if(neighbours[i] != NULL){
      neighbours[i]->set_writing(true);
      while(neighbours[i]->queue.remaining() != 0)
        neighbours[i]->queue.doWrite();
      
      neighbours[i]->set_writing(false);
    }
  }
}
    
void AreaEngine::flushControllers(){
  //Force all Claim messages to controllers in this step!
  for (vector<EpollConnection*>::const_iterator it = 
       controllers.begin(); it != controllers.end(); it++) {
    while((*it)->queue.remaining() != 0)
      (*it)->queue.doWrite(); 
  }
}

void AreaEngine::forceUpdates(){
  MessageType type;
	int len;
	const void *buffer;

  //FORCE all updates FROM neighbors to be entered before we start the step (necessary for synchronization)
  for(int i = 0; i < 8; i++)
  {
    if(neighbours[i] != NULL){
      neighbours[i]->set_reading(true);
      while(neighbours[i]->reader.doRead(&type, &len, &buffer))
      {
        switch (type) {
			    case MSG_SERVERROBOT: {
			      ServerRobot serverrobot;
			      serverrobot.ParseFromArray(buffer, len);
			      GotServerRobot(serverrobot);
			      break;
			    }
			    case MSG_PUCKSTACK: {
			      PuckStack puckstack;
			      puckstack.ParseFromArray(buffer, len);
			      GotPuckStack(puckstack);
			    }

		      default:
				  cerr << "Unexpected readable message from Region\n";
				  break;
			  }
      }
      
      neighbours[i]->set_reading(false);
    }
  }
}

//these are the robot puck handling methods - if the action is not possible they will
//gracefully fail. Since puck updates don't require sync precision, we can notify best effort
//Also, this method is blunt right now. Just pick up any pick beneath you.
void AreaEngine::PickUpPuck(int robotId){

  //find the robot
  RobotObject * curRobot = robots.find(robotId)->second;
  
  if(curRobot->holdingPuck) //already have a puck
    return;
 
  if(RemovePuck(curRobot->x, curRobot->y))  //can pick up a puck
  {
    curRobot->holdingPuck = true;
  }
   
}

void AreaEngine::DropPuck(int robotId){
  //find the robot
  RobotObject * curRobot = robots.find(robotId)->second;
  
  if(!curRobot->holdingPuck) //dont have a puck
    return;
  
  //Add the puck
  AddPuck(curRobot->x, curRobot->y);
  curRobot->holdingPuck = false;
  
}

//set a puck stack in the system.
void AreaEngine::SetPuckStack(double newx, double newy, int newc){
  Index puckElement = getRobotIndices(newx, newy, true);
  
  ArrayObject *element = &robotArray[puckElement.x][puckElement.y];
  
  int x = (int) newx;
  int y = (int) newy;
  
  PuckStackObject * curStack = element->pucks;
  PuckStackObject * lastStack = NULL;
  //iterate through the pucks, looking for a match
  while(curStack != NULL)
  {
    if(curStack->x == x && curStack->y == y)
      break;
      
    lastStack = curStack;
    curStack = curStack->nextStack;
  }
  
  if(newc == 0){
  
    if(lastStack == NULL) //its the first one
      element->pucks = curStack->nextStack;
    else{
      lastStack->nextStack = curStack->nextStack;
    }
  
    puckq.erase(curStack);
    delete curStack;
    
  }else{
  
    if(curStack == NULL)
    {
      //new stack
      curStack = new PuckStackObject(x, y);
      curStack->nextStack = element->pucks;
      element->pucks = curStack;
      puckq[curStack] = true;
    }
    
    curStack->count = newc;
  }
}

//add a puck to the system.
void AreaEngine::AddPuck(double newx, double newy){
  Index puckElement = getRobotIndices(newx, newy, true);
  
  ArrayObject *element = &robotArray[puckElement.x][puckElement.y];
  
  int x = (int) newx;
  int y = (int) newy;
  
  PuckStackObject * curStack = element->pucks;
  //iterate through the pucks, looking for a match
  while(curStack != NULL)
  {
    if(curStack->x == x && curStack->y == y)
      break;
    curStack = curStack->nextStack;
  }
  
  if(curStack == NULL)
  {
    //new stack
    curStack = new PuckStackObject(x, y);
    curStack->nextStack = element->pucks;
    element->pucks = curStack;
    puckq[curStack] = true;
  }else{
    //increment
    curStack->count++;
  }
  
  BroadcastPuckStack(curStack);
}

//remove a puck - returns a success boolean. This way we can just call the method when a client
//requests and not bother checking ourselves. Tries to remove a puck within the pickup range of x and y
bool AreaEngine::RemovePuck(double x, double y){

  Index topLeft = getRobotIndices(x-pickupRange, y-pickupRange, true);
  Index botRight = getRobotIndices(x+pickupRange, y+pickupRange, true);
  
  double tlx = x-pickupRange;
  double tly = y-pickupRange;
  double brx = x+pickupRange;
  double bry = y+pickupRange;
  
  //do puck and robot sight acquires
  for(int j = topLeft.x; j <= botRight.x; j++)
    for(int k = topLeft.y; k <= botRight.y; k++)
    {      
      ArrayObject *element = &robotArray[j][k];

      PuckStackObject * curStack = element->pucks;      
      PuckStackObject * lastStack = NULL;
      
      //iterate through the pucks, looking for a match
      while(curStack != NULL)
      {
        if(curStack->x >= tlx && curStack->y >= tly && curStack->x <= brx && curStack->y <= bry)
          break;

        lastStack = curStack;
        curStack = curStack->nextStack;
      }
      
      if(curStack == NULL)
        continue;
       
      curStack->count--;
      
      BroadcastPuckStack(curStack);
      
      //check it for deletion
      if(curStack->count == 0)
      {
        if(lastStack == NULL) //its the first one
          element->pucks = curStack->nextStack;
        else{
          lastStack->nextStack = curStack->nextStack;
        }
        puckq.erase(curStack);
        delete curStack;
      }

      return true;
    }
  
  return false;
}

//add a robot to the system.
//overload
void AreaEngine::AddRobot(RobotObject * oldRobot){
  //find where it belongs in a[][] and add it
  ArrayObject *element = &robotArray[oldRobot->arrayLocation.x][oldRobot->arrayLocation.y];

  //check if the area is empty first
  if(element->robots == NULL)
  {
    element->robots = oldRobot;
    oldRobot->nextRobot = NULL;
  }else{
    oldRobot->nextRobot = element->robots;
    element->robots = oldRobot;
  }
  
}

RobotObject* AreaEngine::AddRobot(int robotId, double newx, double newy, double newa, double newvx, double newvy, int atStep, int teamId, bool broadcast){
  //O(1) insertion  
  Index robotIndices = getRobotIndices(newx, newy, true);
  
  RobotObject* newRobot = new RobotObject(robotId, newx, newy, newa, newvx, newvy, robotIndices, atStep, teamId);

  //add the robot to our robots vector (used for timestepping)
  robots.insert(pair<int, RobotObject*>(robotId, newRobot));
  
  //find where it belongs in a[][] and add it
  ArrayObject *element = &robotArray[robotIndices.x][robotIndices.y];
  //check if the area is empty first
  if(element->robots == NULL)
  {
    element->robots = newRobot;
    newRobot->nextRobot = NULL;
  }else{
    newRobot->nextRobot = element->robots;
    element->robots = newRobot;
  }
  
  if(broadcast)
    BroadcastRobot(newRobot, Index(regionBounds/2, regionBounds/2), robotIndices, atStep);
  
  return newRobot;
}

//remove a robot with id robotId from the a[xInd][yInd] array element. cleanup. returns true if a robot was deleted
void AreaEngine::RemoveRobot(int robotId, int xInd, int yInd, bool freeMem){
  //O(1) Deletion. Would be O(m), but m (robots in area) is bounded by a constant, so actually O(1)

  ArrayObject *element = &robotArray[xInd][yInd];
  //check if the area is empty first
  if(element->robots == NULL)
    throw SystemError("AreaEngine: Remove Robot In-method failure (1)");

  RobotObject *curRobot = element->robots;
  //check it if its first
  if(curRobot->id == robotId)
  {
    element->robots = curRobot->nextRobot;
    if(freeMem){
      delete curRobot;
    }
    return;
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
      return;
    }
    lastRobot = curRobot;
    curRobot = lastRobot->nextRobot;
  }
  
  throw SystemError("AreaEngine: Remove Robot In-method failure (2)");
}

bool AreaEngine::WeControlRobot(int robotId) {
  if(robots.find(robotId) == robots.end())
    return false;
  Index indices = robots[robotId]->arrayLocation;
  return (!(indices.x < 1 || indices.y < 1 || indices.x > regionBounds 
      || indices.y > regionBounds));
}

bool AreaEngine::ChangeVelocity(int robotId, double newvx, double newvy){
  // Check if we don't control the robot's grid cell.
  if (!WeControlRobot(robotId))
    return false; 

  if(isnan(newvx) || isnan(newvy))
    throw runtime_error("AreaEngine: received NaN new velocity");

  //enforce max speed
  double len = sqrt(newvx*newvx + newvy*newvy);
  if(len > maxSpeed)
  {
    newvx *= (maxSpeed/len);
    newvy *= (maxSpeed/len);
  }
  
  //determine applied step
  int appliedStep = curStep+1;
  if(appliedStep % 2 == 0)
    appliedStep++;
  
  if(appliedStep == 0)
    throw SystemError("AreaEngine: AppliedStep = 0. This should never happen.");
    
  //store it
  Command *newCommand = new Command();
  
  newCommand->robotId = robotId;
  newCommand->step = appliedStep;
  
  newCommand->velocityx = newvx;
  newCommand->velocityy = newvy;

  clientChangeQueue.push(newCommand);
  
  return true;
}

//NOTE: newangle is not the desired angle, it is the angle amount to rotate by!
//NOTE: this method is unfinished and incorrect.
bool AreaEngine::ChangeAngle(int robotId, double newangle){
  if (!WeControlRobot(robotId))
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
    robots[robotId]->angle += 2*M_PI;
    
  //BroadcastRobot(robots[robotId], Index(regionBounds/2, regionBounds/2), robots[robotId]->arrayLocation);
     
  return true;
}

//tell the areaengine that a socket handle is open to write updates to
void AreaEngine::SetNeighbour(int placement, EpollConnection *socketHandle){
  neighbours[placement] = socketHandle;
}

void AreaEngine::GotPuckStack(PuckStack message){
  SetPuckStack(message.x(), message.y(), message.stacksize());
}

void AreaEngine::GotServerRobot(ServerRobot message){
  if(robots.find(message.id()) == robots.end()){
    //new robot
    if(message.laststep() < curStep-1)
    {
      //please comment spammy debugs before commiting
      //cout << "Received out of sync serverrobot add (dropping it): msgstep " << message.laststep() << ", curstep " << curStep << ", x " << message.x() << ", y " << message.y() << ", marker " << marker << endl;
      return;
    }
    
    AddRobot(message.id(), message.x(), message.y(), message.angle(), message.velocityx(), message.velocityy(), message.laststep(), message.team(), false);
  }else{
    //modify existing;

    //store all changes, and purge during step
    if(message.laststep() < curStep-1)
    {
      //please comment spammy debugs before commiting
      //cout << "Received out of sync serverrobot mod (dropping it): msgstep " << message.laststep() << ", curstep " << curStep << ", x " << message.x() << ", y " << message.y() << ", marker " << marker << endl;
      
      //throw "AreaEngine: ServerRobot received late. Impossible desync occurred.";
      return;
    }else{
      //store it
      Command *newCommand = new Command();
      
      newCommand->robotId = message.id();
      newCommand->step = message.laststep();
      
      if(message.has_velocityx()) 
        newCommand->velocityx = message.velocityx();
        
      if(message.has_velocityy()) 
        newCommand->velocityy = message.velocityy();
        
      if(message.has_angle()) 
        newCommand->angle = message.angle();
      
      serverChangeQueue.push(newCommand);
    }
  }
}

void AreaEngine::AddController(EpollConnection *socketHandle){
  controllers.push_back(socketHandle);
}

void AreaEngine::BroadcastRobot(RobotObject *curRobot, Index oldIndices, Index newIndices, int step){
  ServerRobot informNeighbour;
  informNeighbour.set_id(curRobot->id);
  //setup to translate the x and y
  int transx = curRobot->x;
  int transy = curRobot->y;
  //continue
  informNeighbour.set_angle(curRobot->angle);
  informNeighbour.set_team(curRobot->team);
  informNeighbour.set_velocityx(curRobot->vx);
  informNeighbour.set_velocityy(curRobot->vy);
    
  informNeighbour.set_laststep(step);
  

  if(newIndices.x == 1 && neighbours[LEFT] != NULL){
    if(oldIndices.x > 1){
      informNeighbour.set_x(transx + regionRatio);
      informNeighbour.set_y(transy);
      neighbours[LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
	  }
		if(newIndices.y == 1 && neighbours[TOP_LEFT] != NULL){
		  if(oldIndices.x > 1 || oldIndices.y > 1){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);;
	    }
    }else if(newIndices.y == regionBounds && neighbours[BOTTOM_LEFT] != NULL){
      if(oldIndices.x > 1 || oldIndices.y < regionBounds){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_LEFT]->queue.push(MSG_SERVERROBOT, informNeighbour);
	    }
    }
  }else if(newIndices.x == regionBounds && neighbours[RIGHT] != NULL){
    if(oldIndices.x < regionBounds){
      informNeighbour.set_x(transx - regionRatio);
      informNeighbour.set_y(transy);
      neighbours[RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
		}
		if(newIndices.y == 1 && neighbours[TOP_RIGHT] != NULL){
		  if(oldIndices.x < regionBounds || oldIndices.y > 1){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
	    }
    }else if(newIndices.y == regionBounds && neighbours[BOTTOM_RIGHT] != NULL){
      if(oldIndices.x < regionBounds || oldIndices.y < regionBounds){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_RIGHT]->queue.push(MSG_SERVERROBOT, informNeighbour);
	    }
    }
  }
  if(newIndices.y == 1 && neighbours[TOP] != NULL){
    if(oldIndices.y > 1){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy + regionRatio);
      neighbours[TOP]->queue.push(MSG_SERVERROBOT, informNeighbour);
	  }
  }else if(newIndices.y == regionBounds && neighbours[BOTTOM] != NULL){
    if(oldIndices.y < regionBounds){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy - regionRatio);
      neighbours[BOTTOM]->queue.push(MSG_SERVERROBOT, informNeighbour);
    }
  }
}

void AreaEngine::BroadcastPuckStack(PuckStackObject *curStack){
  PuckStack informNeighbour;
  //setup to translate the x and y
  int transx = curStack->x;
  int transy = curStack->y;
  //continue
  informNeighbour.set_stacksize(curStack->count);
  Index atIndices = getRobotIndices(curStack->x, curStack->y, true);
  

  if(atIndices.x == 1 && neighbours[LEFT] != NULL){
      informNeighbour.set_x(transx + regionRatio);
      informNeighbour.set_y(transy);
      neighbours[LEFT]->queue.push(MSG_PUCKSTACK, informNeighbour);
		if(atIndices.y == 1 && neighbours[TOP_LEFT] != NULL){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_LEFT]->queue.push(MSG_PUCKSTACK, informNeighbour);
    }else if(atIndices.y == regionBounds && neighbours[BOTTOM_LEFT] != NULL){
        informNeighbour.set_x(transx + regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_LEFT]->queue.push(MSG_PUCKSTACK, informNeighbour);
    }
  }else if(atIndices.x == regionBounds && neighbours[RIGHT] != NULL){
      informNeighbour.set_x(transx - regionRatio);
      informNeighbour.set_y(transy);
      neighbours[RIGHT]->queue.push(MSG_PUCKSTACK, informNeighbour);
		if(atIndices.y == 1 && neighbours[TOP_RIGHT] != NULL){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy + regionRatio);
        neighbours[TOP_RIGHT]->queue.push(MSG_PUCKSTACK, informNeighbour);
    }else if(atIndices.y == regionBounds && neighbours[BOTTOM_RIGHT] != NULL){
        informNeighbour.set_x(transx - regionRatio);
        informNeighbour.set_y(transy - regionRatio);
        neighbours[BOTTOM_RIGHT]->queue.push(MSG_PUCKSTACK, informNeighbour);
    }
  }
  if(atIndices.y == 1 && neighbours[TOP] != NULL){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy + regionRatio);
      neighbours[TOP]->queue.push(MSG_PUCKSTACK, informNeighbour);
  }else if(atIndices.y == regionBounds && neighbours[BOTTOM] != NULL){
      informNeighbour.set_x(transx);
      informNeighbour.set_y(transy - regionRatio);
      neighbours[BOTTOM]->queue.push(MSG_PUCKSTACK, informNeighbour);
  }
}
