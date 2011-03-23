#include "client.h"

class Forager : public ClientAi {
public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// Forager robot. Pick up any pucks we can. Don't worry about enemy robots.

		// Are we interested in EVENT_CAN_PICKUP_PUCK event?
		// Check if we are on a puck. If so, just pick it up.
		//DONT for now.
		if (hasCanPickUpPuckEvent(ownRobot)) {
			SeenPuck* pickup = findPickUpablePuck(ownRobot);
			if (pickup != NULL) {
        command.setPuckPickup(true);
				return;
			}
		}

		// Check if we are not moving
		if (hasNotMovingEvent(ownRobot)) {
			command.setVx(((rand() % 11) / 10.0) - 0.5);
			command.setVy(((rand() % 11) / 10.0) - 0.5);
			return;
		}

		// Make robot move in direction of the nearest puck.
		SeenPuck* closest = findClosestPuck(ownRobot);
		if (closest != NULL) {
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

			double velocity = 0.1;
			if (relDistance(closest->relx, closest->rely) < 1.0) {
				velocity = 0.01;
			}
			if (closest->relx <= 0.0) {
				// Move left!
				command.setVx(velocity * -1.0 * modx);
			} else if (closest->relx > 0.0) {
				command.setVx(velocity * modx);
			}
			if (closest->rely <= 0.0) {
				// Move up!
				command.setVy(velocity * -1.0 * mody);
			} else if (closest->rely > 0.0) {
				command.setVy(velocity * mody);
			}
		}
	}
};

extern "C" {
ClientAi* maker() {
	return new Forager;
}
}
