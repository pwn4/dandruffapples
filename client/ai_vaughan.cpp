#include "client.h"

class Vaughan : public ClientAi {
public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
    // Behaviour: 
    //   1. If we have no puck
    //      -Try to pickup a puck, if it is not in our home
    //      -Move towards the nearest puck
    //      -Randomly change velocity every x timesteps until we see puck
    //   2. If we have a puck
    //      -If we are in our home, drop the puck
    //      -Move towards our home

		if (!ownRobot->hasPuck) {
			SeenPuck* pickup = findPickUpablePuck(ownRobot);
			if (pickup != NULL && !insideOurHome(ownRobot)) {
        // We're on a puck, we think! Set velocity to 0 and pickup.
				command.sendCommand = true;
				command.changePuckPickup = true;
				command.puckPickup = true;
        command.changeVx = true;
        command.vx = 0.0;
        command.changeVy = true;
        command.vx = 0.0;
			} else {
        // Make robot move in direction of the nearest puck.
        SeenPuck* closest = findClosestPuck(ownRobot);
        if (closest != NULL) {
          // We can see at least one puck!
          if(closest->rely == 0)  //prevent NaN
          closest->rely = 0.000001;
          double ratio = abs(closest->relx / closest->rely);
          double modx = 1.0;
          double mody = 1.0;
          if (ratio > 1.0) {
            mody = 1.0 / ratio;
          } else {
            modx = ratio;
          }

          double velocity = 1.0;
          if (relDistance(closest->relx, closest->rely) < 1.0) {
            // The closer we are to the puck, the slower we move!
            velocity = 0.01;
          }
          command.sendCommand = true;
          if (closest->relx <= 0.0) {
            // Move left!
            command.changeVx = true;
            command.vx = velocity * -1.0 * modx;
          } else if (closest->relx > 0.0) {
            command.changeVx = true;
            command.vx = velocity * modx;
          }
          if (closest->rely <= 0.0) {
            // Move up!
            command.changeVy = true;
            command.vy = velocity * -1.0 * mody;
          } else if (closest->rely > 0.0) {
            command.changeVy = true;
            command.vy = velocity * mody;
          }
        } else {
          // We can't see any pucks! Move if we are "not moving".
          if (abs(ownRobot->vx) < 0.1 && abs(ownRobot->vy) < 0.1) {
            // We're moving too slow! New random velocity.
            command.sendCommand = true;
            command.changeVx = true;
            command.vx = (((rand() % 11) / 10.0) - 0.5);
            command.changeVy = true;
            command.vy = (((rand() % 11) / 10.0) - 0.5);
          }
        }
      }
		} else {
      // We have a puck! 
      //do /3 to ensure we're really inside the home before dropping. None of this border crap.
			if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY) < HOMEDIAMETER/3) {
        // We're home! Drop the puck. 
				command.sendCommand = true;
				command.changePuckPickup = true;
				command.puckPickup = false;
        command.changeVx = true;
        command.vx = 0.0;
        command.changeVy = true;
        command.vx = 0.0;
			} else {
        // Carry the puck home!
        if(ownRobot->homeRelY == 0)  //prevent NaN
          ownRobot->homeRelY = 0.000001;
        double ratio = abs(ownRobot->homeRelX / ownRobot->homeRelY);
        double modx = 1.0;
        double mody = 1.0;
        if (ratio > 1.0) {
          mody = 1.0 / ratio;
        } else {
          modx = ratio;
        }

        double velocity = 1.0;
        command.sendCommand = true;
        if (ownRobot->homeRelX <= 0.0) {
          // Move left!
          command.changeVx = true;
          command.vx = velocity * -1.0 * modx;
        } else if (ownRobot->homeRelX > 0.0) {
          command.changeVx = true;
          command.vx = velocity * modx;
        }
        if (ownRobot->homeRelY <= 0.0) {
          // Move up!
          command.changeVy = true;
          command.vy = velocity * -1.0 * mody;
        } else if (ownRobot->homeRelY > 0.0) {
          command.changeVy = true;
          command.vy = velocity * mody;
        }
      }
    }
    return;
	}
};

extern "C" {
ClientAi* maker() {
	return new Vaughan;
}
}
