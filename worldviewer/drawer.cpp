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

//this is the color mapping method: teams->Color components
ColorObject colorFromTeam(int teamId){
  //initialize the color map for the first time
  if(colorMapInitialized == false)
  {
    colorMap = new ColorObject[65535];
    
    for(int i = 0; i < 65534; i++)
      colorMap[i] = new ColorObject(0.01*(rand() % 60) + 0.2, 0.01*(rand() % 60) + 0.2, 0.01*(rand() % 60) + 0.2);
      
    //set puck color
    colorMap[65534] = new ColorObject(0, 0, 0);
    
    colorMapInitialized = true;
  }

  return colorMap[teamId];
}

cairo_surface_t * UnpackImage(RegionRender render)
{
  //double buffer
  cairo_surface_t *stepImage;
  cairo_t *stepImageDrawer;
  
  //init our surface
  stepImage = cairo_image_surface_create (IMAGEFORMAT , IMAGEWIDTH, IMAGEHEIGHT);
  stepImageDrawer = cairo_create (stepImage);

  //clear the image
  cairo_set_source_rgb (stepImageDrawer, 1, 1, 1);
  cairo_paint (stepImageDrawer);

  //perhaps just for now, outline the region's boundaries. That way we can see them in the viewer
  cairo_rectangle (stepImageDrawer, 0, 0, IMAGEWIDTH, IMAGEHEIGHT);
  cairo_set_source_rgb(stepImageDrawer, .5, .5, .5);
  cairo_stroke (stepImageDrawer);

  int curY = 0;
  for(int i = 0; i < render.image_size(); i++)
  {
    TwoInt curRobot = ByteUnpack(render.image(i));

    while(curRobot.one == 65535 && curRobot.two == 65535 && i < render.image_size()){
      i++;
      if(i >= render.image_size())
        break;
      curRobot = ByteUnpack(render.image(i));
      curY++;
    }

    if(i >= render.image_size())
      break;

    cairo_rectangle (stepImageDrawer, curRobot.one, curY, 1, 1);
    //set the color
    ColorObject robotColor = colorFromTeam(curRobot.two);

    cairo_set_source_rgb(stepImageDrawer, robotColor.r, robotColor.g, robotColor.b);

    cairo_fill (stepImageDrawer);
  }
  
  cairo_destroy(stepImageDrawer);

  return stepImage;
}
