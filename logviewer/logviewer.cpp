// logviewer.cpp

#include <GL/glut.h>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <string>
//#include <stdlib.h>
//#include <cstdio>
//#include <cstdlib>
//#include <fstream>
//#include <sys/socket.h>
#include <sys/fcntl.h>

#include "logviewer.h"
#include "robot.h"
#include "home.h"
#include "puck.h"

#include "../common/messagereader.h"
#include "../common/timestep.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
#include "../common/types.h"

using namespace std;

int Logviewer::winsize = 600;
double Logviewer::worldsize = 1.0;
vector<Robot*> Logviewer::robots;
vector<Home*> Logviewer::homes;
vector<Puck*> Logviewer::pucks;

MessageReader *reader; 
TimestepUpdate timestep;
ServerRobot serverrobot;
PuckStack puckstack;

Logviewer::Logviewer(int worldLength, int worldHeight) 
  : _worldLength(worldLength), _worldHeight(worldHeight) {
  // Initialize window size to top left corner 
  _winHeight = 500;
  _winLength = 500;
  _winStartX = 0;
  _winStartY = 0;
}

void Logviewer::getInitialData() {
  // Load log file
  int fd = 0;
  string filename = "antix_log";

  fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    cerr << "Unable to open test log file!" << endl;
    exit(1);
  }

  reader = new MessageReader(fd);

  // Hardcode robot/puck info below this point 
  // determine which indexes correspond with which robots
  int robotsPerTeam = 5;
  int teams = 3;
  int numPucks = 20;

  // Now parse initial world data to assign robots to vector
  Robot* tempRobot = NULL;
  Home* tempHome = NULL;
  Puck* tempPuck = NULL;
  for (int i = 0; i < teams; i++) {
    tempHome = new Home(1.0 / (i + 2), 1.0 / (i + 2), 0.2, i);
    Logviewer::homes.push_back(tempHome);
    for (int j = 0; j < robotsPerTeam; j++) {
      tempRobot = new Robot(1.0 / (i + j + 2), 1.0 / (i + j + 2), i); // arbitrary position
      robots.push_back(tempRobot); 
    }
  }

  for (int i = 0; i < numPucks; i++) {
    tempPuck = new Puck(1.0 / (i + 2), 0.5, 1);
    pucks.push_back(tempPuck);
  }
}

void Logviewer::updateTimestep() {
  bool keepGoing = true;

  MessageType type;
  size_t len;
  const void* buffer;

  while(keepGoing) {
    if (reader->doRead(&type, &len, &buffer)) {
      usleep(0.2);
      switch(type) {
      case MSG_TIMESTEPUPDATE:
        timestep.ParseFromArray(buffer, len);
        cout << "got timestep " << timestep.timestep() << endl;
        keepGoing = false;
        break;
      case MSG_SERVERROBOT:
        serverrobot.ParseFromArray(buffer, len);
        cout << "got serverrobot " << serverrobot.id(); 
        if (serverrobot.has_x()) {
          cout << " at x=" << serverrobot.x() << endl;
        } else {
          cout << " with no position data\n";
        }
        break;
      case MSG_PUCKSTACK:
        puckstack.ParseFromArray(buffer, len);
        cout << "got puckstack size " << puckstack.stacksize() 
             << " at x=" << puckstack.x() << endl;
        break;
      default:
        cerr << "Unknown message!" << endl;
        break;
      }
    }
  }


/*
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
*/
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
