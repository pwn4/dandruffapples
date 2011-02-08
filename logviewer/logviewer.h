#ifndef _LOGVIEWER_H_
#define _LOGVIEWER_H_

#include <vector>
#include <math.h>
#include "robot.h"
#include "home.h"
#include "puckstack.h"

#define VAR(V,init) __typeof(init) V=(init)
#define FOR_EACH(I,C) for(VAR(I,(C).begin());I!=(C).end();I++)

using namespace std;

class Logviewer {
private:

public:
  // Entire world size
  int _worldLength;
  int _worldHeight;

  // View window
  int _winLength;
  int _winHeight;
  int _winStartX;
  int _winStartY;
  static double worldsize; // fix this
  static int winsize; // fix this

  // Robots, homes, and pucks
  static vector<Robot*> robots;
  static vector<Home*> homes;
  static vector<PuckStack*> puckStacks;
  
  // Methods

  Logviewer(int worldLength, int worldHeight);
  static void getInitialData(); 
  //void run();
  static void updateTimestep();

  static double distanceNormalize(double d);
  static double angleNormalize(double a);

  /** Convert radians to degrees. */
  static double rtod(double r);
  /** Convert degrees to radians */
  static double dtor(double d);
};

#endif //_LOGVIEWER_H_
