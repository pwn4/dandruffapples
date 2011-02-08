// logviewer.cpp

#include <GL/glut.h>
#include <vector>
#include <unistd.h>

#include "logviewer.h"
#include "robot.h"
#include "home.h"
#include "puckstack.h"

int Logviewer::winsize = 600;
double Logviewer::worldsize = 1.0;
vector<Robot*> Logviewer::robots;
vector<Home*> Logviewer::homes;
vector<PuckStack*> Logviewer::puckStacks;

Logviewer::Logviewer(int worldLength, int worldHeight) 
  : _worldLength(worldLength), _worldHeight(worldHeight) {
  // Initialize window size to top left corner 
  _winHeight = 500;
  _winLength = 500;
  _winStartX = 0;
  _winStartY = 0;
}

void Logviewer::getInitialData() {
  // from config file?
  // Hardcode for now...

  // determine which indexes correspond with which robots
  int robotsPerTeam = 5;
  int teams = 3;
  int pucks = 20;

  // Now parse initial world data to assign robots to vector
  Robot* tempRobot = NULL;
  Home* tempHome = NULL;
  PuckStack* tempPuckStack = NULL;
  for (int i = 0; i < teams; i++) {
    tempHome = new Home(1.0 / (i + 2), 1.0 / (i + 2), 0.2, i);
    Logviewer::homes.push_back(tempHome);
    for (int j = 0; j < robotsPerTeam; j++) {
      tempRobot = new Robot(1.0 / (i + j + 2), 1.0 / (i + j + 2), i); // arbitrary position
      robots.push_back(tempRobot); 
    }
  }

  for (int i = 0; i < pucks; i++) {
    tempPuckStack = new PuckStack(1.0 / (i + 2), 0.5, 1);
    puckStacks.push_back(tempPuckStack);
  }
}

void Logviewer::updateTimestep() {
  // 1. Parse ServerRobot and PuckStack messages for current timestep.
  // 2. For all robots we did not receive a message for, calculate new
  //    x,y coordinates based on current velocity.

  // But until we get log files... let's make them DANCE!
  double dx, dy;
  Robot* bot = NULL;
  for (int i = 0; i < robots.size(); i++) {
    bot = robots.at(i);
    bot->_velocity = 0.0001;
    dx = bot->_velocity * cos(bot->_angle);
    dy = bot->_velocity * sin(bot->_angle);
    bot->_x = distanceNormalize(bot->_x + dx); 
    bot->_y = distanceNormalize(bot->_y + dy); 
  }
  usleep(0.1);
  return;
}

/** Normalize a length to within 0 to worldsize. */
double Logviewer::distanceNormalize(double d) {
	while( d < 0 ) d += worldsize;
	while( d > worldsize ) d -= worldsize;
	return d; 
} 

/** Normalize an angle to within +/_ M_PI. */
double Logviewer::angleNormalize(double a) {
	while( a < -M_PI ) a += 2.0*M_PI;
	while( a >  M_PI ) a -= 2.0*M_PI;	 
	return a;
}	 

double Logviewer::rtod(double r) {
  return (r * 180.0 / M_PI);
}

double Logviewer::dtor(double d) {
  return (d * M_PI / 180.0);
}
