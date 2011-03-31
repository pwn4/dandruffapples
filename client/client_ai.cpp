#include "client.h"

#define LEFTMOST 		1
#define RIGHTMOST 	2
#define TOPMOST			3
#define BOTTOMMOST	4
#define ANY					5


SeenPuck* ClientAi::findClosestPuck(OwnRobot* ownRobot) {
	if (ownRobot->seenPucks.size() <= 0)
		return NULL;

	vector<SeenPuck*>::iterator closest;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	bool found = false;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		tempDistance = relDistance((*it)->relx, (*it)->rely);
		//Don't return pucks in our home
		if(puckInsideOurHome((*it), ownRobot)){
			continue;
		}
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			closest = it;
			found = true;
		}
	}
	if(!found){
		return NULL;
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
	bool found = false;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		tempDistance = relDistance((*it)->relx, (*it)->rely);
		//Don't return pucks in our home
		if(puckInsideOurHome((*it), ownRobot) || (*it) == closest){
			continue;
		}
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			secondClosest = it;
			found = true;
		}
	}

	if(!found){
		return NULL;
	}
	return *secondClosest;
}

SeenPuck* ClientAi::findPickUpablePuck(OwnRobot* ownRobot) { //TODO:this function seems weird...robot could be over 2 pucks at same time? 
	if (ownRobot->seenPucks.size() == 0)
		return NULL;

	vector<SeenRobot*>::iterator closest;
	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
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

//Are we standing on pucks?
bool ClientAi::canPickUpPuck(OwnRobot* ownRobot) {
	if (ownRobot->seenPucks.size() == 0){
		return false;
	}

	for (vector<SeenPuck*>::iterator it = ownRobot->seenPucks.begin(); it != ownRobot->seenPucks.end(); it++) {
		if(relDistance((*it)->relx, (*it)->rely) <= ROBOTDIAMETER/2){
			return true;
		}
	}

	return false;
}

//Am I closest to this puck?
bool ClientAi::ImClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck) {
	SeenRobot* closest = robotClosestToPuck(ownRobot, seenPuck);
	if(closest == NULL){// I think I'm closest to the puck
		return true;
	}
	else{
		double hisDist = relDistance(closest->relx - seenPuck->relx, closest->rely - seenPuck->rely);
		double myDist = relDistance(seenPuck->relx, seenPuck->rely);
		return (myDist < hisDist);
	}
}
//Am I STRICTLY SECOND closest to the puck?
bool ClientAi::ImSecondClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck){
	if (ownRobot->seenRobots.size() == 0){//I see no other robots. I AM THE CLOSEST
	}
	else if(robotSecondClosestToPuck(ownRobot, seenPuck) == NULL){
		return true;
	}
	return false;
}

//Which robot is closest to this puck?
SeenRobot* ClientAi::robotClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck) {
	if (ownRobot->seenRobots.size() == 0){ //I think I'm closest to the puck
		return NULL;
	}
	vector<SeenRobot*>::iterator closest;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
		tempDistance = relDistance((*it)->relx - seenPuck->relx, (*it)->rely - seenPuck->rely);
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			closest = it;
		}
	}
	return *closest;
}

//Which robot is second closest to this puck?
//Return: NULL if I am second closest or there is no second closest robot
//Return: SeenRobot second closest to puck otherwise
SeenRobot* ClientAi::robotSecondClosestToPuck(OwnRobot* ownRobot, SeenPuck* seenPuck) {
	if (ownRobot->seenRobots.size() == 0){ //There is no other robot around to be second closest
		return NULL;
	}

	vector<SeenRobot*>::iterator closest2;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	bool ImClosest = true;
	SeenRobot* closest = NULL;
	if(ImClosestToPuck(ownRobot, seenPuck)){//I think I'm closest to puck. Second closest is in SeenRobot
	}
	else{//some other robot is closest to the puck. so who's second closest?
		ImClosest = false;
		closest = robotClosestToPuck(ownRobot, seenPuck);
	}

	for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
		if(!ImClosest && ((*it) == closest)){//skip the closet robot if I'm not the closest
			continue;
		}
		tempDistance = relDistance((*it)->relx - seenPuck->relx, (*it)->rely - seenPuck->rely);
		if (tempDistance < minDistance) {
			minDistance = tempDistance;
			closest2 = it;
		}
	}
	//Got potential second closest robot.
	//So, either I was the closest, then second closest is a SeenRobot
	//OR some other robot was closest, and now either another robot is second closest or I am.
	if(ImClosest){//I can't be the second closest
		return *closest2;
	}
	else{
		double hisDist = relDistance((*closest2)->relx - seenPuck->relx, (*closest2)->rely - seenPuck->rely);
		double myDist = relDistance(seenPuck->relx, seenPuck->rely);
		if(myDist < hisDist){//I'm second closest!
			return NULL;
		}
		else{
			return *closest2;
		}
	}
}

//Get the closest robot to me in every direction
SeenRobot* ClientAi::findClosestRobot(OwnRobot* ownRobot) {
	return closestRobotDirection(ownRobot, ANY);
}

//Is robot dist distance away from home?
bool ClientAi::closeToHome(OwnRobot* ownRobot, double dist){
	if(relDistance(ownRobot->homeRelX, ownRobot->homeRelY) <= dist){
		return true;
	}
	else{
		return false;
	}
}

//Is robot at home?
bool ClientAi::insideOurHome(OwnRobot* ownRobot) {
  return (relDistance(ownRobot->homeRelX, ownRobot->homeRelY) <= HOMEDIAMETER/2);
}

//Is it an enemy robot?
bool ClientAi::isEnemy(SeenRobot* seenRobot) {
  return !weControlRobot(seenRobot->id);
}

//How many pucks are around me?
int ClientAi::canSeeNumPucks(OwnRobot* ownRobot){
	return ownRobot->seenPucks.size();
}

//STRICTLY LEFT Ignores robots on top and bottom with closer relative x coordinates
int ClientAi::numRobotsLeftOfMe(OwnRobot* ownRobot){
	return numRobotsDirection(ownRobot, LEFTMOST);
}

//STRICTLY RIGHT Ignores robots on top and bottom with closer relative x coordinates
int ClientAi::numRobotsRightOfMe(OwnRobot* ownRobot){
	return numRobotsDirection(ownRobot, RIGHTMOST);
}

//STRICTLY TOP Ignores robots on right and left with closer relative y coordinates
int ClientAi::numRobotsTopOfMe(OwnRobot* ownRobot){
	return numRobotsDirection(ownRobot, TOPMOST);
}

//STRICTLY BOTTOM Ignores robots on right and left with closer relative y coordinates
int ClientAi::numRobotsBottomOfMe(OwnRobot* ownRobot){
	return numRobotsDirection(ownRobot, BOTTOMMOST);
}

//Number of robots in my sight of view
int ClientAi::canSeeNumRobots(OwnRobot* ownRobot){
	return ownRobot->seenRobots.size();
}

//STRICTLY LEFTMOST robot that I can see. Ignores robots on top and bottom with closer relative x coordinates
SeenRobot* ClientAi::leftmostRobotToMe(OwnRobot* ownRobot){
	return closestRobotDirection(ownRobot, LEFTMOST);
}

//STRICTLY RIGHTMOST robot that I can see. Ignores robots on top and bottom with closer relative x coordinates
SeenRobot* ClientAi::rightmostRobotToMe(OwnRobot* ownRobot){
	return closestRobotDirection(ownRobot, RIGHTMOST);
}

//STRICTLY TOPMOST robot that I can see. Ignores robots on left and right with closer relative y coordinates
SeenRobot* ClientAi::topmostRobotToMe(OwnRobot* ownRobot){
	return closestRobotDirection(ownRobot, TOPMOST);
}

//STRICTLY BOTTOMMOST robot that I can see. Ignores robots on left and right with closer relative y coordinates
SeenRobot* ClientAi::bottommostRobotToMe(OwnRobot* ownRobot){
	return closestRobotDirection(ownRobot, BOTTOMMOST);
}

//helper function for finding number of robots in some direction
int ClientAi::numRobotsDirection(OwnRobot* ownRobot, int direction){
	int numRobots = 0;
	if (ownRobot->seenRobots.size() == 0){
		return numRobots;
	}
	for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
		switch(direction)
		{
			case LEFTMOST:
			{
				if((*it)->relx + ROBOTDIAMETER <=0){ //this is where the strictness comes in
					numRobots++;
				}
				break;
			}
			case RIGHTMOST:
			{
				if((*it)->relx - ROBOTDIAMETER >=0){ //this is where the strictness comes in
						numRobots++;
				}
				break;
			}
			case TOPMOST:
			{
				if((*it)->rely + ROBOTDIAMETER <=0){ //this is where the strictness comes in
						numRobots++;
				}
				break;
			}
			case BOTTOMMOST:
			{
				if((*it)->rely - ROBOTDIAMETER >=0){ //this is where the strictness comes in
						numRobots++;
				}
				break;
			}
			default:
			{
				return 0;
			}
		}
	}
	return numRobots;
}

//helper function for finding closest robot in some direction
SeenRobot* ClientAi::closestRobotDirection(OwnRobot* ownRobot, int direction){
	if (ownRobot->seenRobots.size() == 0){
		return NULL;
	}
	
	vector<SeenRobot*>::iterator target;
	double minDistance = 9000.01; // Over nine thousand!
	double tempDistance;
	bool found = false;

	for (vector<SeenRobot*>::iterator it = ownRobot->seenRobots.begin(); it != ownRobot->seenRobots.end(); it++) {
		switch(direction)
		{
			case LEFTMOST:
			{
				if((*it)->relx + ROBOTDIAMETER <=0){ //this is where the strictness comes in
					tempDistance = relDistance((*it)->relx, (*it)->rely);
					if (tempDistance < minDistance) {
						minDistance = tempDistance;
						target = it;
						found = true;
					}
				}
				break;
			}
			case RIGHTMOST:
			{
				if((*it)->relx - ROBOTDIAMETER >=0){ //this is where the strictness comes in
					tempDistance = relDistance((*it)->relx, (*it)->rely);
					if (tempDistance < minDistance) {
						minDistance = tempDistance;
						target = it;
						found = true;
					}
				}
				break;
			}
			case TOPMOST:
			{
				if((*it)->rely + ROBOTDIAMETER <=0){ //this is where the strictness comes in
					tempDistance = relDistance((*it)->relx, (*it)->rely);
					if (tempDistance < minDistance) {
						minDistance = tempDistance;
						target = it;
						found = true;
					}
				}
				break;
			}
			case BOTTOMMOST:
			{
				if((*it)->rely - ROBOTDIAMETER >=0){ //this is where the strictness comes in
					tempDistance = relDistance((*it)->relx, (*it)->rely);
					if (tempDistance < minDistance) {
						minDistance = tempDistance;
						target = it;
						found = true;
					}
				}
				break;
			}
			case ANY:
			{
				tempDistance = relDistance((*it)->relx, (*it)->rely);
				if (tempDistance < minDistance) {
					minDistance = tempDistance;
					target = it;
					found = true;
				}
				break;
			}
			default:
			{
				return NULL;
			}	
		}
	}

	if(found){
		return *target;
	}
	else{
		return NULL;
	}
}

//Does robot think puck is in our home?
bool ClientAi::puckInsideOurHome(SeenPuck* seenPuck, OwnRobot* ownRobot) {
  return (relDistance(ownRobot->homeRelX - seenPuck->relx, ownRobot->homeRelY - seenPuck->rely) <= HOMEDIAMETER/2);
}

double ClientAi::relDistance(double x1, double y1) {
	return (sqrt(x1 * x1 + y1 * y1));
}

double ClientAi::distance(double x1, double y1, double x2, double y2) {
	return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}

// Check if the two coordinates are the same, compensating for doubleing-point errors.
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

pair<double, double> ClientAi::polar2cartesian(double rho, double theta) {
	double x = rho * cos(theta);
	double y = rho * sin(theta);
	return make_pair(x, y);
}

double ClientAi::degree2radian(double d) {
	return d * PI / 180;
}

pair<double, double> ClientAi::normalize(pair<double, double> coords, double scale) {
	if (coords.first == 0.0 && coords.second == 0.0) {
		return make_pair(0, 0);
	} else {
		double l = sqrt(coords.first*coords.first + coords.second*coords.second);
		return make_pair(coords.first/l*scale, coords.second/l*scale);
	}
}
