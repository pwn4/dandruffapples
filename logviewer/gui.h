// gui.h

#ifndef _GUI_H_
#define _GUI_H_

#include "logviewer.h"
#include "robot.h"

class Gui {
public:
  static Logviewer* _logviewer;

  // Methods
  static void setLogviewer(Logviewer* logviewer);
  static void idle_func(void);
  static void timer_func(int dummy);
  static void display_func(void);
  static void mouse_func(int button, int state, int x, int y);
  static void glDrawCircle(double x, double y, double r, double count);
  static void initGraphics(int argc, char* argv[]);
  static void updateGui();
  static void drawAll();
  static void draw(Robot* r);

  //Gui();
};

#endif //_GUI_H_
