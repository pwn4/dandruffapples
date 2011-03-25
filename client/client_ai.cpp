#include "client.h"

SeenPuck* ClientAi::findClosestPuck(OwnRobot* ownRobot) {
	if (ownRobot->seenPucks.size() <= 0)
		return NULL;

	vector<SeenPuck*>::iterator closest;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		tempDistance = relDistance((*it)->relx, (*it)->rely);
		//Don't return pucks in our home
		if(puckInsideOurHome((*it), ownRobot)){
			continue;
		}
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			closest = it;
		}
	}
	return *closest;
}

SeenPuck* ClientAi::findSecondClosestPuck(OwnRobot* ownRobot) {
	if (ownRobot->seenPucks.size() < 2)
		return NULL;

	SeenPuck* closest = findClosestPuck(ownRobot);

	vector<SeenPuck*>::iterator secondClosest;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		tempDistance = relDistance((*it)->relx, (*it)->rely);
		//Don't return pucks in our home
		if(puckInsideOurHome((*it), ownRobot) || (*it) == closest){
			continue;
		}
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			secondClosest = it;
		}
	}
	return *secondClosest;
}

// Checks whether the robot is inside its home.
bool ClientAi::insideOurHome(OwnRobot* ownRobot) {
  return (relDistance(ownRobot->homeRelX, ownRobot->homeRelY) <= HOMEDIAMETER/2);
}

// Checks whether the robot thinks puck is inside its home.
bool ClientAi::puckInsideOurHome(SeenPuck* seenPuck, OwnRobot* ownRobot) {
  return (relDistance(ownRobot->homeRelX - seenPuck->relx, ownRobot->homeRelY - seenPuck->rely) <= HOMEDIAMETER/2);
}

SeenPuck* ClientAi::findPickUpablePuck(OwnRobot* ownRobot) { //TODO:this function seems weird...robot could be over 2 pucks at same time? 
	if (ownRobot->seenPucks.size() == 0)
		return NULL;

	vector<SeenRobot*>::iterator closest;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		//this is wrong! This is TOO STRICT
		//if (sameCoordinates((*it)->relx, (*it)->rely, 0.0, 0.0)) {
		//make the AIs go in a little closer as a buffer
		//Also, don't include pucks inside the home as pickupable pucks
/*
		if(abs((*it)->relx) < ROBOTDIAMETER/2 - 0.2 && abs((*it)->rely) < ROBOTDIAMETER/2 - 0.2 && relDistance((*it)->relx - ownRobot->homeRelX, (*it)->rely - ownRobot->homeRelY) > HOMEDIAMETER/2) {
			return *it;
		}
*/
		if(puckInsideOurHome(*it, ownRobot)){
			continue;
		}
		else{
			if(relDistance((*it)->relx, (*it)->rely) <= ROBOTDIAMETER/2){
				return *it;
			}
		}
	}
	return NULL; // Found nothing in the for loop.
}

SeenRobot* ClientAi::findClosestRobot(OwnRobot* ownRobot) {
	if (ownRobot->seenRobots.size() == 0)
		return NULL;

	vector<SeenRobot*>::iterator closest;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
		tempDistance = relDistance((*it)->relx, (*it)->rely);
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			closest = it;
		}
	}
	return *closest;
}

double ClientAi::relDistance(double x1, double y1) {
	return (sqrt(x1 * x1 + y1 * y1));
}

// Check if the two coordinates are the same, compensating for
// doubleing-point errors.
bool ClientAi::sameCoordinates(double x1, double y1, double x2, double y2) {
	// From testing, it looks like doubleing point errors either add or subtract
	// 0.0001.
	double maxError = 0.1;
	if (abs(x1 - x2) > maxError) {
		return false;
	}
	if (abs(y1 - y2) > maxError) {
		return false;
	}
	return true;
}

bool ClientAi::hasCanPickUpPuckEvent(OwnRobot* ownRobot) {
	for (vector<EventType>::iterator it = ownRobot->eventQueue.begin(); it != ownRobot->eventQueue.end(); it++) {
		if (*it == EVENT_CAN_PICKUP_PUCK)
			return true;
	}
	return false;
}

bool ClientAi::hasNotMovingEvent(OwnRobot* ownRobot) {
	for (vector<EventType>::iterator it = ownRobot->eventQueue.begin(); it != ownRobot->eventQueue.end(); it++) {
		if (*it == EVENT_NOT_MOVING)
			return true;
	}
	return false;
}
