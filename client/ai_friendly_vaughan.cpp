#include "client.h"

class Vaughan: public ClientAi {
private:
	double defaultSpeed;

	void nextDestination(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// when we have destinations
		while (ownRobot->hasDestination()) {
			pair<double, double> top = ownRobot->destFront();
			// if i'm already at the destination, ignore it
			if (abs(-ownRobot->homeRelX-top.first) + abs(-ownRobot->homeRelY-top.second) < 0.5) {
				ownRobot->destPopFront();
				ownRobot->focus = false;
				command.setVx(0.0);
				command.setVy(0.0);
				return;
			} else {
			// else, go towards the destination
				pair<double, double> d = make_pair(top.first+ownRobot->homeRelX, top.second+ownRobot->homeRelY);
				double dist = distance((-1)*ownRobot->homeRelX, (-1)*ownRobot->homeRelY, top.first, top.second);
				double s = min(defaultSpeed, dist/20);
				if (dist < 5.0)
					s = 0.1;
				pair<double, double> v = normalize(d, s);

				command.setVx(v.first);
				command.setVy(v.second);

				return;
			}
		}

		// we don't have destinations
		// TODO: maybe choose a puck that's closest to our home as the next destination?
		// for now, randomly move
		pair<double, double> v = randomMove(command, defaultSpeed);

		pair<double, double> newdest = make_pair(50 * v.first-ownRobot->homeRelX, 50 * v.second-ownRobot->homeRelY);
		ownRobot->destPush(newdest);
	}

	pair<double, double> randomMove(ClientRobotCommand& command, double speed){
		pair<double, double> v = polar2cartesian(speed, degree2radian(rand()%360));
		command.setVx(v.first);
		command.setVy(v.second);
		return v;
	}

	bool noSpeed(OwnRobot* ownRobot) {
		return abs(ownRobot->vx) < 0.0001 && abs(ownRobot->vy) < 0.0001;
	}

public:
	Vaughan() : defaultSpeed(5.0) {}
	void path_home(OwnRobot* ownRobot, int zone) {
		if (zone == 0) {
			// not zone information yet
			zone = get_zone(ownRobot);
		}
		switch (zone) {
		case 1:
			path_home(ownRobot, 4);
			break;
		case 2:
			path_home(ownRobot, 1);
			break;
		case 3:
			path_home(ownRobot, 4);
			break;
		case 4:
			break;
		}
		pair<double, double> dest = get_dest(zone);
		ownRobot->destPush(dest);
	}

	int get_zone(OwnRobot* ownRobot) {
		/*
		 * calculate which zone the robot is in
		 *		| 	2	|
		 *		|_______|
		 * 		|		|
		 *  1	| home 	| 3
		 * _____|_______|_____
		 *
		 * 			4
		 *
		 * */
		if (((-1)*ownRobot->homeRelY-0.5*HOMEDIAMETER) > 0){
			return 4;
		}
		if (((-1)*ownRobot->homeRelX - 0.5*HOMEDIAMETER) > 0 ){
			return 3;
		}
		if (((-1)*ownRobot->homeRelX + 0.5*HOMEDIAMETER) < 0 ){
			return 1;
		}
		return 2;

	}

	pair<double, double> get_dest(int zone) {
		/*
		 * get a random number inside the destination zone for robot to go
		 * dest2 | 		 |
		 * ______|_______|
		 * 		 |       |
		 *   	 | home  |
		 * ______|_______|______
		 *|	 	 |		 |	    |
		 *|dest1 | dest4 | dest3|
		 *|______|_______|______|
		 * */
		double ranx = rand()%HOMEDIAMETER;
		double rany = rand()%HOMEDIAMETER;
		pair<double,double> dest;
		switch(zone){
		case 1:
			dest = make_pair(ranx-5.5*HOMEDIAMETER, rany + 5.5*HOMEDIAMETER);
			break;
		case 2:
			dest = make_pair(ranx-3.5*HOMEDIAMETER, rany - 5.5*HOMEDIAMETER);
			break;
		case 3:
			dest = make_pair(ranx + 3.5*HOMEDIAMETER, rany + 5.5*HOMEDIAMETER);
			break;
		case 4:
			dest = make_pair(ranx + 0.5*HOMEDIAMETER, rany + 2.5*HOMEDIAMETER);
			break;
		}
		return dest;
	}

	void make_command(ClientRobotCommand& command, OwnRobot* ownRobot) {
		decide(command, ownRobot);

		// adjust speed so that I won't collide with some robot ahead
		SeenRobot* closestRobot = findClosestRobot(ownRobot);
		if (closestRobot != NULL) {
			double dist = relDistance(closestRobot->relx, closestRobot->rely);
			double m = relDistance(command.vx, command.vy);
			if (dist < 10 * m) {
				m = dist / 10;
				pair<double, double> v = make_pair(command.vx, command.vy);
				normalize(v, m);
				command.setVx(v.first);
				command.setVy(v.second);
			}
		}
	}

	void decide(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// SMARTER AI
		//
		/*
		 *  If robot in home and has no puck
		 * 		go up
		 *  If robot has no puck and sees a puck:
		 * 		go to the puck
		 *  else, follow the destinations
		 *
		 *
		 *  When pick up a puck:
		 * 		set a set of destination (Path to enter home from bottom to top)
		 * 	When in home and has puck:
		 * 		drop it
		 *
		 */

		if (!ownRobot->hasPuck) {
			if (relDistance(ownRobot->vx, ownRobot->vy) < 4 * HOMEDIAMETER && ownRobot->vy < 0) {
				// go up when robot is inside home and has dropped puck
				if (noSpeed(ownRobot)) {
					randomMove(command, 0.2);
				} else {
					command.setVy(-1);
					if (ownRobot->vy < -0.2 * HOMEDIAMETER) {
						pair<double, double> v = make_pair((-1)*ownRobot->homeRelX, (-1)*ownRobot->homeRelY);
						normalize(v, defaultSpeed);
						command.setVx(v.first);
						command.setVy(v.second);
						ownRobot->clearDestinations();
						ownRobot->focus = false;
					}
				}
				return;
			}
			SeenPuck* pickup = findPickUpablePuck(ownRobot);
			if (pickup != NULL) {
				// We're on a puck, we think! Set velocity to 0 and pickup.
				command.setPuckPickup(true);
				command.setVx(0.0);
				command.setVy(0.0);
				ownRobot->focus = false;
				return;
			} else {
				if (ownRobot->focus) {
					// do not change destination
				} else {
					// go to the next nearest puck

					SeenPuck* closestPuck = findClosestPuck(ownRobot);
					if (closestPuck != NULL && !ownRobot->focus) {
						pair<double, double> newdest(closestPuck->relx-ownRobot->homeRelX,
								closestPuck->rely-ownRobot->homeRelY);
						// push the next destination
						ownRobot->destPush(newdest);
						ownRobot->focus = true;
						//TODO push all seen pucks to the known puck list
					}
				}
				nextDestination(command, ownRobot);
			}
		} else {
		// has puck
			if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY)
					< HOMEDIAMETER / 2.2) {
				// Robot at home, drop puck
				command.setPuckPickup(false);
				ownRobot->has_path_home = false;
				ownRobot->focus = false;
				command.setVx(0.0);
				command.setVy(0.5);
				ownRobot->destPopFront();
			} else {
			// Robot outside home
				if (!ownRobot->has_path_home) {
					// calculate a path to go inside home from bottom
					path_home(ownRobot, 0);
					ownRobot->has_path_home = true;
				}
			}
			if (!ownRobot->focus) {
				if ((-1)*ownRobot->homeRelY > 0.2 * HOMEDIAMETER && (-1)*ownRobot->homeRelY < 4 * HOMEDIAMETER) {
					ownRobot->clearDestinations();
					pair<double, double> v = make_pair(0, 0);
					ownRobot->destPush(v);
					ownRobot->focus = true;
				}
				nextDestination(command, ownRobot);
			} else {
				// manually guide robot to home

					if ((-1)*ownRobot->homeRelX < -0.3 * HOMEDIAMETER) {
						// go right
						command.setVy(0);
						command.setVx(2);
					} else if ((-1)* ownRobot->homeRelX > 0.3 * HOMEDIAMETER ) {
						// go left
						command.setVy(0);
						command.setVx(-2);
					} else {
						// go up
						command.setVy(-1);
						command.setVx(0);
					}

					SeenRobot* closestRobot = findClosestRobot(ownRobot);
					if (closestRobot) {
						double dist = relDistance(closestRobot->relx, closestRobot->rely);
						if (dist < 1.5*ROBOTDIAMETER) {
							command.setVx(command.vx/50);
							command.setVy(command.vy/50);
							int d = rand() % 100;
							if (d > 50) {
								randomMove(command, 0.15);
							}
						}

					}

				return;
				/*
				nextDestination(command, ownRobot);
				if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY) < 5 * HOMEDIAMETER) {


					// we're stuck on the way to home, and we're close to home
					if (ownRobot->vx < 0.0001 && ownRobot->vy < 0.0001) {
						// randomly wait
						randomMove(command);
						//ownRobot->has_path_home = false;
					}
					return;
				}
				*/
			}

		}

		// it's good except when we collides
		if (abs(ownRobot->vx) < 0.0001 && abs(ownRobot->vy) < 0.0001) {

			if (ownRobot->hasDestination()) {
				// it have target, turn to right for 60 degrees
				pair<double, double> top = ownRobot->destFront();
				pair<double, double> v = make_pair(ownRobot->homeRelX+top.first,
											       ownRobot->homeRelY+top.second);
				// rotate randomly
				double r = degree2radian(rand()%180 - 90 + 1);
				pair<double, double> newv = make_pair(v.first*cos(r)-v.second*sin(r),
													  v.first*sin(r)+v.second*cos(r));
				command.setVx(newv.first);
				command.setVy(newv.second);
			} else {
				// go wherever
				double velocity = defaultSpeed;
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

		return;

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
