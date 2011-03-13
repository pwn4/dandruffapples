#include "drawer.h"

//Takes Two Integers and Packs their first 16 bits into 4 bytes.
//Note: Is NOT cross platform ('course neither is epoll)
//ASSUMES BIG ENDIAN, FOR NOW
unsigned int BytePack(int a, int b) {

  unsigned int clear = 65535;
  unsigned int rtn = a;
  rtn = rtn & clear;

  unsigned int tmpb = b;
  tmpb = tmpb & clear;
  tmpb = tmpb << 16;

  rtn = rtn | tmpb;

  return rtn;

}

//ASSUMES BIG ENDIAN, FOR NOW
TwoInt ByteUnpack(unsigned int data) {

  unsigned int clear = 65535;
  unsigned int first = data & clear;

  unsigned int second = data >> 16;
  second = second & clear;

  return TwoInt(first, second);

}

//globals
ColorObject coloringMap[65535];
bool colorMapInitialized = false;

//this is the color mapping method: teams->Color components
ColorObject colorFromTeam(int teamId){
  //initialize the color map for the first time
  if(colorMapInitialized == false)
  {
    //use the same seed always so that our colors are the same everywhere
    srand(1);

    for(int i = 0; i < 65534; i++)
      coloringMap[i] = ColorObject(0.01*(rand() % 60) + 0.2, 0.01*(rand() % 60) + 0.2, 0.01*(rand() % 60) + 0.2);

    //set puck color
    coloringMap[65534] = ColorObject(0, 0, 0);

    colorMapInitialized = true;
  }

  return coloringMap[teamId];
}

void UnpackImage(cairo_t *cr, RegionRender* render, int robotSize, double robotAlpha)
{
  int curY = 0;
  for(int i = 0; i < render->image_size(); i++)
  {
    TwoInt curRobot = ByteUnpack(render->image(i));

    while(curRobot.one == 65535 && curRobot.two == 65535 && i < render->image_size()){
      i++;
      if(i >= render->image_size())
        break;
      curRobot = ByteUnpack(render->image(i));
      curY++;
    }

    if(i >= render->image_size())
      break;

    //set the color
    ColorObject robotColor = colorFromTeam(curRobot.two);

    //pixel precision robot drawing
    if(robotSize == 1){
      cairo_set_source_rgba(cr, robotColor.r, robotColor.g, robotColor.b, robotAlpha);
      cairo_rectangle (cr, curRobot.one, curY, robotSize, robotSize);
      cairo_fill (cr);
    }else{
    	cairo_pattern_t *radpat;  //for drawing gradients with large robots
    //aliased precision
        //init the gradient
      radpat = cairo_pattern_create_radial (curRobot.one, curY, 0.0,  curRobot.one, curY, (double)robotSize / 2.0);
      cairo_pattern_add_color_stop_rgba (radpat, 0,  robotColor.r, robotColor.g, robotColor.b, robotAlpha);
      cairo_pattern_add_color_stop_rgba (radpat, (double)robotSize / 2.0,  robotColor.r, robotColor.g, robotColor.b, 0.0);

      cairo_rectangle (cr, curRobot.one - (robotSize / 2.0), curY - (robotSize / 2.0), robotSize, robotSize);
      cairo_set_source (cr, radpat);
      cairo_fill (cr);
      cairo_pattern_destroy(radpat);
    }

  }

}
