#include "client.h"

class Vaughan: public ClientAi {
public:
	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// Behaviour:
		//   1. If we have no puck
		//      -Try to pickup a puck, if it is not in our home
		//      -Move towards the nearest puck .DOESNT SEEM TO RECALL MEMORY OF NEAREST PUCK
		//      -Randomly change velocity every x timesteps until we see puck
		//   2. If we have a puck
		//      -If we are in our home, drop the puck
		//      -Move towards our home
		if (!ownRobot->hasPuck) {
			SeenPuck* pickup = findPickUpablePuck(ownRobot);
			if (pickup != NULL && !insideOurHome(ownRobot)) {
				// We're on a puck, we think! Set velocity to 0 and pickup.
				command.setPuckPickup(true);
				command.setVx(0.0);
				command.setVy(0.0);
			} else {
				// Make robot move in direction of the nearest puck.
				SeenPuck* closest = findClosestPuck(ownRobot);
				SeenRobot* closestRobot = findClosestRobot(ownRobot);
				if (closest != NULL) {
					// We can see at least one puck!
					if (closest->rely == 0) //prevent NaN
						closest->rely = 0.000001;
					double ratio = abs(closest->relx / closest->rely);
					double modx = 1.0;
					double mody = 1.0;
					if (ratio > 1.0) {
						mody = 1.0 / ratio;
					} else {
						modx = ratio;
					}

					if (closestRobot == NULL || (closestRobot != NULL
							&& (relDistance(closestRobot->relx,
									closestRobot->rely) > 1.5 * ROBOTDIAMETER))) {
						//noone is close to us
						double velocity = 1.0;
						if (relDistance(closest->relx, closest->rely) < 5.0) {
							// The closer we are to the puck, the slower we move!
							velocity = 0.1;
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
					} else {
						//some other robot is closer to the puck, go look for another
						SeenPuck* secondClosest = findSecondClosestPuck(
								ownRobot);
						if (secondClosest != NULL) {
							// We can see at least one puck!
							if (secondClosest->rely == 0) //prevent NaN
								secondClosest->rely = 0.000001;
							double ratio = abs(
									secondClosest->relx / secondClosest->rely);
							double modx = 1.0;
							double mody = 1.0;
							if (ratio > 1.0) {
								mody = 1.0 / ratio;
							} else {
								modx = ratio;
							}

							double velocity = 1.0;
							if (relDistance(closest->relx, closest->rely) < 5.0) {
								// The closer we are to the puck, the slower we move!
								velocity = 0.1;
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

						} else {
							//or backoff from it
							double velocity = ((rand() % 11) / 10.0);
							;
							//move away!
							if (closestRobot->relx > 0.0) {
								// Move right!
								command.setVx(velocity * -1.0 * modx);
							} else {
								command.setVx(velocity * modx);
							}
							if (closestRobot->rely > 0.0) {
								// Move down!
								command.setVy(velocity * -1.0 * mody);
							} else {
								command.setVy(velocity * mody);
							}
						}
					}
				} else {
					// We can't see any pucks! Move if we are "not moving".
					if (abs(ownRobot->vx) < 0.3 && abs(ownRobot->vy) < 0.3) {
						// We're moving too slow! New random velocity.
						double velocity = 1.0;
						double modx = 1.0;
						double mody = 1.0;
						//move away!
						if (rand() % 3 == 0) {
							// Move right!
							command.setVx(velocity * -1.0 * modx);
						} else {
							command.setVx(velocity * modx);
						}
						if (rand() % 4 == 0) {
							// Move down!
							command.setVy(velocity * -1.0 * mody);
						} else {
							command.setVy(velocity * mody);
						}
					}
				}
			}
		} else if (ownRobot->hasPuck) {
			// We have a puck!
			//do /3.5 to ensure we're really inside the home before dropping. None of this border crap.
			if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY)
					< HOMEDIAMETER / 2.2) {
				// We're home! Drop the puck.
				command.setPuckPickup(false);
				command.setVx(0.0);
				command.setVy(0.0);
			} else {
				// Carry the puck home!
				SeenRobot* closestRobot = findClosestRobot(ownRobot);
				if (ownRobot->homeRelY == 0) //prevent NaN
					ownRobot->homeRelY = 0.000001;
				double ratio = abs(ownRobot->homeRelX / ownRobot->homeRelY);
				double modx = 1.0;
				double mody = 1.0;
				if (ratio > 1.0) {
					mody = 1.0 / ratio;
				} else {
					modx = ratio;
				}
				if (closestRobot == NULL || (closestRobot != NULL
						&& (relDistance(closestRobot->relx, closestRobot->rely)
								> 1.5 * ROBOTDIAMETER))) {
					double velocity = 1.0;
					if (ownRobot->homeRelX <= 0.0) {
						// Move left!
						command.setVx(velocity * -1.0 * modx);
					} else if (ownRobot->homeRelX > 0.0) {
						command.setVx(velocity * modx);
					}
					if (ownRobot->homeRelY <= 0.0) {
						// Move up!
						command.setVy(velocity * -1.0 * mody);
					} else if (ownRobot->homeRelY > 0.0) {
						command.setVy(velocity * mody);
					}
				} else if ((closestRobot != NULL && (relDistance(
						closestRobot->relx, closestRobot->rely) < 1.5
						* ROBOTDIAMETER))) {
					//something is blocking us...try to slightly differenty but in same direction
					int randNum = rand();
					double velocity = ((randNum % 11) / 10.0);
					if (randNum % 4 > 1) {
						if (ownRobot->homeRelX <= 0.0 && randNum % 5 > 1) {
							// Move left!
							command.setVx(velocity * -1.0 * modx);
						} else {
							command.setVx(velocity * modx);
						}
					}
					if (randNum % 4 > 2) {
						if (ownRobot->homeRelY <= 0.0 && randNum % 5 > 1) {
							// Move up!
							command.setVy(velocity * -1.0 * mody);
						} else {
							command.setVy(velocity * mody);
						}
					}
				}
			}
		} else if (insideOurHome(ownRobot)) {
			//get out of the house!
			double velocity = 1.0;
			double modx = 1.0;
			double mody = 1.0;
			if (ownRobot->homeRelX < HOMEDIAMETER / 2) {
				// Move left!
				command.setVx(velocity * -1.0 * modx);
			} else if (ownRobot->homeRelX >= HOMEDIAMETER / 2) {
				command.setVx(velocity * modx);
			}
			if (ownRobot->homeRelY < HOMEDIAMETER / 2) {
				// Move up!
				command.setVy(velocity * -1.0 * mody);
			} else if (ownRobot->homeRelY >= HOMEDIAMETER / 2) {
				command.setVy(velocity * mody);
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
