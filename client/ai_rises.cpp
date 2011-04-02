#include "client.h"

class Rises : public ClientAi {

void goHome(ClientRobotCommand& command, OwnRobot* ownRobot);
void leaveHome(ClientRobotCommand& command, OwnRobot* ownRobot);
int goLeft(ClientRobotCommand& command, OwnRobot* ownRobot);
int goRight(ClientRobotCommand& command, OwnRobot* ownRobot);
int goUp(ClientRobotCommand& command, OwnRobot* ownRobot);
int goDown(ClientRobotCommand& command, OwnRobot* ownRobot);

public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
		if(ownRobot->hasPuck && !insideOurHome(ownRobot)){
			goHome(command,ownRobot);
		}
		else if(ownRobot->hasPuck && insideOurHome(ownRobot)){
			// We're home! Drop the puck.
			command.setVx(0.0);
			command.setVy(0.0);
			command.setPuckPickup(false);
		}
		else if(!ownRobot->hasPuck && insideOurHome(ownRobot)){
			leaveHome(command, ownRobot);
		}
		else if(!ownRobot->hasPuck) {//find a puck
			if (canPickUpPuck(ownRobot) && !insideOurHome(ownRobot)) {//Found a puck to bring home!
        // We're on a puck, we think! Set velocity to 0 and pickup.
        command.setVx(0.0);
        command.setVy(0.0);
				command.setPuckPickup(true);
			}
			else{//Time to find a puck!
        // Make robot move in direction of the nearest free puck.
        SeenPuck* closestPuck = findClosestPuck(ownRobot);//closest puck
				if((ownRobot->vx == 0.0) && (ownRobot->vy == 0.0)){//i think i'm stuck...
					if(goUp(command,ownRobot)){
					}
					else if(goRight(command,ownRobot)){
					}
					else if(goDown(command,ownRobot)){
					}
					else if(goLeft(command,ownRobot)){
					}
					else{
						command.setVx(0);
						command.setVy(0);
					}
				}
				else if(closestPuck!= NULL && ImClosestToPuck(ownRobot, closestPuck)){
					//go straight for it!
		      if(closestPuck->rely == 0) //prevent NaN
		      	closestPuck->rely = 0.000001;
					double ratio = abs(closestPuck->relx/closestPuck->rely);
					double modx = 1.0;
					double mody = 1.0;
					double velocity = 1.0;
					if (ratio > 1.0) {
						mody = 1.0 / ratio;
					} else {
						modx = ratio;
					}
		      if (relDistance(closestPuck->relx, closestPuck->rely) < 1.5*ROBOTDIAMETER) {
		        // The closer we are to the puck, the slower we move!
		        velocity = 0.05;
		      }
		      if (closestPuck->relx <= 0.0) {
		        // Move left!
		        command.setVx(velocity * -1.0 * modx);
		      } else if (closestPuck->relx > 0.0) {
		        command.setVx(velocity * modx);
		      }
		      if (closestPuck->rely <= 0.0) {
		        // Move up!
		        command.setVy(velocity * -1.0 * mody);
		      } else if (closestPuck->rely > 0.0) {
		        command.setVy(velocity * mody);
		      }
				}
				else{//go randomly look
					if(goUp(command,ownRobot)){
					}
					else if(goRight(command,ownRobot)){
					}
					else if(goDown(command,ownRobot)){
					}
					else if(goLeft(command,ownRobot)){
					}
					else{
						command.setVx(0);
						command.setVy(0);
					}
				}
			}
		}
    return;
	}
};

int Vaughan::goLeft(ClientRobotCommand& command, OwnRobot* ownRobot){
	SeenRobot* leftmost = leftmostRobotToMe(ownRobot);
	if(leftmost == NULL || (relDistance(leftmost->relx, leftmost->rely) > 2*ROBOTDIAMETER)){
		for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
			if(((*it)->relx < 0) && (abs((*it)->rely) - ROBOTDIAMETER <=0)){
				return 0;
			}
		}
		command.setVx(-1);
		command.setVy(0);
		return 1;
	}
	return 0;
}
int Vaughan::goRight(ClientRobotCommand& command, OwnRobot* ownRobot){
	SeenRobot* rightmost = rightmostRobotToMe(ownRobot);
	if(rightmost == NULL || (relDistance(rightmost->relx, rightmost->rely) > 2*ROBOTDIAMETER)){
		for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
			if(((*it)->relx > 0) && (abs((*it)->rely) - ROBOTDIAMETER <=0)){
				return 0;
			}
		}
		command.setVx(1);
		command.setVy(0);
		return 1;
	}
	return 0;
}
int Vaughan::goUp(ClientRobotCommand& command, OwnRobot* ownRobot){
	SeenRobot* topmost = topmostRobotToMe(ownRobot);
	if(topmost == NULL || (relDistance(topmost->relx, topmost->rely) > 2*ROBOTDIAMETER)){
		for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
			if(((*it)->rely < 0) && (abs((*it)->relx) - ROBOTDIAMETER <=0)){
				return 0;
			}
		}
		command.setVx(0);
		command.setVy(-1);
		return 1;
	}
	return 0;
}
int Vaughan::goDown(ClientRobotCommand& command, OwnRobot* ownRobot){
		SeenRobot* bottommost = bottommostRobotToMe(ownRobot);
	if(bottommost == NULL || (relDistance(bottommost->relx, bottommost->rely) > 2*ROBOTDIAMETER)){
		for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
			if(((*it)->rely > 0) && (abs((*it)->relx) - ROBOTDIAMETER <=0)){
				return 0;
			}
		}
		command.setVx(0);
		command.setVy(1);
		return 1;
	}
	return 0;
}

void Vaughan::goHome(ClientRobotCommand& command, OwnRobot* ownRobot){
		double velocity = 1;
		double x = ownRobot->homeRelX;
		double y = ownRobot->homeRelY;
    double ratio = abs(x/y);
    double modx = 1.0;
    double mody = 1.0;
    if (ratio > 1.0) {
      mody = 1.0 / ratio;
    } else {
      modx = ratio;
    }

//		double angle = getAngle(x, y);
		if(closeToHome(ownRobot, 15*HOMEDIAMETER)){//start lining up at entrances
			if(y>0){//coming from the top
				if(x>0){//coming from the topleft
					if(abs(x) <= 1.5*HOMEDIAMETER){//go left or down or up or right
						if(goLeft(command, ownRobot)){
						}
						else if(goDown(command, ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else{//go down or left or up or right
						if(goDown(command, ownRobot)){
						}
						else if(goLeft(command, ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
				}
				else{//coming from the topright
					if(abs(x) <= 1.5*HOMEDIAMETER){//go right or down or up or left
						if(goRight(command,ownRobot)){
						}
						else if(goDown(command, ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else{//go down or right or up or left
						if(goDown(command, ownRobot)){
						}
						else if(goRight(command, ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
				}
			}
			else{//we're at the bottom
				if(x>0){//coming from the bottomleft
					if(abs(x) <= HOMEDIAMETER/2.5){//go up or right or down or left
						if(goUp(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else if(goDown(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else if(abs(y) <= HOMEDIAMETER){//go down or right or up or left
						if(goDown(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else{//go right or down or up or left
						if(goRight(command,ownRobot)){
						}
						else if(goDown(command,ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
				}
				else{//coming from the bottomright
					if(abs(x) <= HOMEDIAMETER/2.5){//go up or left or down or right
						if(goUp(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else if(goDown(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else if(abs(y) <= HOMEDIAMETER){//go down or left or up or right
						if(goDown(command,ownRobot)){
						}
						else if(goLeft(command,ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
					else{//go left or down or up or right
						if(goLeft(command,ownRobot)){
						}
						else if(goDown(command,ownRobot)){
						}
						else if(goUp(command,ownRobot)){
						}
						else if(goRight(command,ownRobot)){
						}
						else{
							command.setVx(0);
							command.setVy(0);
						}
					}
				}
			}
		}
		else{
			SeenRobot* rightmost = rightmostRobotToMe(ownRobot);
			SeenRobot* topmost = topmostRobotToMe(ownRobot);
			SeenRobot* bottommost = bottommostRobotToMe(ownRobot);
			SeenRobot* leftmost = leftmostRobotToMe(ownRobot);
      if (x <= 0.0) {// Move left!
        command.setVx(velocity * -1.0 * modx);
				if(leftmost != NULL && relDistance(leftmost->relx, leftmost->rely) < 1.5*ROBOTDIAMETER){
					command.setVx(0);
				}
      } 
			else if (x > 0.0) {
        command.setVx(velocity * modx);
				if(rightmost != NULL && relDistance(rightmost->relx, rightmost->rely) < 1.5*ROBOTDIAMETER){
					command.setVx(0);
				}
      }
      if (y <= 0.0) {// Move up!
        command.setVy(velocity * -1.0 * mody);
				if(topmost != NULL && relDistance(topmost->relx, topmost->rely) < 1.5*ROBOTDIAMETER){
					command.setVy(0);
				}
      } 
			else if (y > 0.0) {
        command.setVy(velocity * mody);
				if(bottommost != NULL && relDistance(bottommost->relx, bottommost->rely) < 1.5*ROBOTDIAMETER){
					command.setVy(0);
				}
      }
		}

}

void Vaughan::leaveHome(ClientRobotCommand& command, OwnRobot* ownRobot){
		double x = ownRobot->homeRelX;
		double y = ownRobot->homeRelY;
		if(y>0){//coming from the top
			if(x>0){//coming from the topleft
			//go up or left or right or down
				if(goUp(command,ownRobot)){
				}
				else if(goLeft(command,ownRobot)){
				}
				else if(goRight(command,ownRobot)){
				}
				else if(goDown(command,ownRobot)){
				}
				else{
					command.setVx(0);
					command.setVy(0);
				}
			}
			else{//coming from the topright
			//go up or right or left or down
				if(goUp(command,ownRobot)){
				}
				else if(goRight(command,ownRobot)){
				}
				else if(goLeft(command,ownRobot)){
				}
				else if(goDown(command,ownRobot)){
				}
				else{
					command.setVx(0);
					command.setVy(0);
				}
			}
		}
		else{//coming from the bottom
			if(x>0){//coming from the bottomleft
				//go up or left or right or down
				if(goUp(command,ownRobot)){
				}
				else if(goLeft(command,ownRobot)){
				}
				else if(goRight(command,ownRobot)){
				}
				else if(goDown(command,ownRobot)){
				}
				else{
					command.setVx(0);
					command.setVy(0);
				}
			}
			else{//coming from the bottomright
			//go up or right or left or down
				if(goUp(command,ownRobot)){
				}
				else if(goRight(command,ownRobot)){
				}
				else if(goLeft(command,ownRobot)){
				}
				else if(goDown(command,ownRobot)){
				}
				else{
					command.setVx(0);
					command.setVy(0);
				}
			}
		}
}

extern "C" {
ClientAi* maker() {
	return new Vaughan;
}
}
