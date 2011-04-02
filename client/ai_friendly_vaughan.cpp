#include "client.h"

class FriendlyVaughan: public ClientAi {
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
		pair<double, double> v = randomMove(command, defaultSpeed, 0, 360);

		pair<double, double> newdest = make_pair(50 * v.first-ownRobot->homeRelX, 50 * v.second-ownRobot->homeRelY);
		ownRobot->destPush(newdest);
	}

	// random move: default rotate angle would be 90~270 to avoid collision
	pair<double, double> randomMove(ClientRobotCommand& command, double speed){
		pair<double, double> v = polar2cartesian(speed, degree2radian(rand()%180+90));
		command.setVx(v.first);
		command.setVy(v.second);
		return v;
	}

	pair<double, double> randomMove(ClientRobotCommand& command, double speed,int angleS, int angleE){
			pair<double, double> v = polar2cartesian(speed, degree2radian(rand()%(angleE-angleS)+angleS));
			command.setVx(v.first);
			command.setVy(v.second);
			return v;
		}

	bool noSpeed(OwnRobot* ownRobot) {
		return abs(ownRobot->vx) < 0.0001 && abs(ownRobot->vy) < 0.0001;
	}

public:
	FriendlyVaughan() : defaultSpeed(5.0) {}
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
			if (dist < 5 * m) {
				m = dist / 5;
				pair<double, double> v = make_pair(command.vx, command.vy);
				normalize(v, m);
				command.setVx(v.first);
				command.setVy(v.second);
			}
		}
	}

	void decide(ClientRobotCommand& command, OwnRobot* ownRobot) {
		// SMARTER AI
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
			// go up when robot is close to home and has no puck
			if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY) < 4 * HOMEDIAMETER
					&& (-1)*ownRobot->homeRelY < 0.7*HOMEDIAMETER) {
				if (noSpeed(ownRobot) && (-1)*ownRobot->homeRelY < 0.5*HOMEDIAMETER) {
					randomMove(command, 0.2);
				} else {
					command.setVy(-2);
					// if still inside home area, go outwards
					if ((-1)*ownRobot->homeRelY < -0.1 * HOMEDIAMETER) {
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
			// try if can pickup puck
			SeenPuck* pickup = findPickUpablePuck(ownRobot);
			// yes we can
			if (pickup != NULL) {
				// We're on a puck, we think! Set velocity to 0 and pickup.
				command.setPuckPickup(true);
				command.setVx(0.0);
				command.setVy(0.0);
				ownRobot->focus = false;
				return;
			} else {
			// no we can't pickup puck
				if (ownRobot->focus) {
					// do not change destination, when go with focus
				} else {
					// locate the nearest puck
					SeenPuck* closestPuck = findClosestPuck(ownRobot);
					if (closestPuck != NULL && !ownRobot->focus) {
						pair<double, double> newdest(closestPuck->relx-ownRobot->homeRelX,
								closestPuck->rely-ownRobot->homeRelY);
						// go towards it!
						ownRobot->destPush(newdest);
						ownRobot->focus = true;
						//TODO push all seen pucks to the known puck list
					}
				}
				nextDestination(command, ownRobot);
			}
		} else {
		// we have a puck
			if (relDistance(ownRobot->homeRelX, ownRobot->homeRelY)
					< HOMEDIAMETER / 2.2) {
				// Robot at home, drop puck, score!
				command.setPuckPickup(false);
				ownRobot->has_path_home = false;
				ownRobot->focus = false;
				command.setVx(0.0);
				command.setVy(-0.5);
				ownRobot->destPopFront();
			} else {
			// Robot outside home
				if (!ownRobot->has_path_home) {
					// calculate a path to go inside home from bottom
					path_home(ownRobot, 0);
					ownRobot->has_path_home = true;
				}
			}

			// Robot on its way to home
			if (!ownRobot->focus) {
				// if ready to go towards home, go with focus
				if ((-1)*ownRobot->homeRelY > 0.3 * HOMEDIAMETER
				 && (-1)*ownRobot->homeRelY < 2.5 * HOMEDIAMETER) {
					ownRobot->clearDestinations();
					pair<double, double> v = make_pair(0, 0);
					ownRobot->destPush(v);
					ownRobot->focus = true;
				}
				nextDestination(command, ownRobot);
			} else {
				// Going directly to home
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
					if (dist < 2*ROBOTDIAMETER) {
						pair<double, double> v = make_pair(command.vx, command.vy);
						command.setVx(command.vx/50);
						command.setVy(command.vy/50);
						int d = rand() % 100;
						if (d > 35) {
							randomMove(command, 0.1, 0, 360);
							//randomMove(command, 0.1);
							double dh = relDistance(ownRobot->homeRelX, ownRobot->homeRelY);
							if (dh < 0.6 * HOMEDIAMETER + 2 * ROBOTDIAMETER) {
								randomMove(command, 0.15, 0, 360);
								//randomMove(command, 0.15);
							}
						}
					}

				}
				return;
			}

		}

		// it's good except when we collide
		if (noSpeed(ownRobot)) {
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
				// random move
				randomMove(command, defaultSpeed);
			}
		}
		return;
	}
};

extern "C" {
ClientAi* maker() {
	return new FriendlyVaughan;
}
}
