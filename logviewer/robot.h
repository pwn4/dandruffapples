#ifndef _ROBOT_H_
#define _ROBOT_H_

class Robot {
public:
  //int id;
  int _teamId;
  float _velocity;
  float _angle;
  bool _hasPuck;
  bool _hasCollided;
  float _x;
  float _y;

  // Methods
  Robot(float x, float y, int teamId);
};

#endif //_ROBOT_H_
