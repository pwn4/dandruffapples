#include "client.h"

class Vaughan : public ClientAi {
public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
    // Behaviour: Move down. Makes robots move almost uniformly across all
    // region servers. Change velocity a little bit whenever AI is called
    // so that we keep sending traffic over the network.
    //   1. If velocity is A, then set it to B.
    //   2. If velocity is B, then set it to A.

    if (ownRobot->vx != 0.0) {
      // Don't move left or right.
      command.setVx(0.0);
    }

    // Alternate vy between 0.49 and 0.51.
    if (ownRobot->vy < 0.5) {
      command.setVy(0.51);
    } else if (ownRobot->vy >= 0.5) {
      command.setVy(0.49);
    }

    return;
	}
};

extern "C" {
ClientAi* maker() {
	return new Vaughan;
}
}
