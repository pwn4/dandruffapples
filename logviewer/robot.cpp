// robot.cpp

#include "robot.h"

Robot::Robot(float x, float y, int teamId) 
  : _x(x), _y(y), _teamId(teamId) {
  _hasPuck = false;
  _hasCollided = false;
  _velocity = 0.0;
  _angle = 0.0;
}
