#include "client.h"

class Scared : public ClientAi {
public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// Scared robot. Run away from all enemy robots.

    // Check if we are not moving
    if (ownRobot->vx == 0.0 && ownRobot->vy == 0.0) {
      command.setVx(((rand() % 11) / 10.0) - 0.5);
      command.setVy(((rand() % 11) / 10.0) - 0.5);
      return;
    }

		// Make robot move in opposite direction. TODO: Add trig!
		SeenRobot* closest = findClosestRobot(ownRobot);
		if (closest != NULL) {
			double velocity = 1.0;
			if (closest->relx <= 0.0) {
				// Move right!
				command.setVx(velocity);
			} else if (closest->relx > 0.0) {
				command.setVx(velocity * -1.0);
			}
			if (closest->rely <= 0.0) {
				// Move down!
				command.setVy(velocity);
			} else if (closest->rely > 0.0) {
				command.setVy(velocity * -1.0);
			}
		}
	}
};

extern "C" {
ClientAi* maker() {
	return new Scared;
}
}
