#include "helper.h"

namespace helper {

string toString(int t) {
	stringstream ss;
	ss << t;

	return ss.str();
}

string getNewName(string base) {
	string name = base;
	struct stat stFileInfo;
	int i = 0;
	int intStat = stat(name.c_str(), &stFileInfo);

	//make sure that the file doesn't exist before we create it
	//if it exists, choose another name
	while (!intStat) {
		name = base + helper::toString(i);
		intStat = stat(name.c_str(), &stFileInfo);
		i++;
	}

	return name;
}

Config::Config(int argc, char* argv[]) {
	//loop through the arguments and parse them into a map
	for (int i = 1; i < argc; i++) {
		//assume that all arguments are provide in "-argType arg" type of way
		if (i % 2) {
			Config::parsedConfig[argv[i]] = "";
			//cout<<"put " << argv[i] <<" into key"<<endl;
		} else {
			Config::parsedConfig[argv[i - 1]] = argv[i];
			//cout<<"put " << argv[i] <<" into key of "<<argv[i-1]<<endl;
		}
	}
}

//return the value of the argument with the type "arg"
string Config::getArg(string arg) {
	return Config::parsedConfig[arg];
}

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
  srand(teamId*123);  //use the same seed for the same team. That way our random numbers are consistent! lol
  return ColorObject(0.01*(rand() % 90), 0.01*(rand() % 90), 0.01*(rand() % 90));
}

void UnpackImage(RegionRender render, cairo_t *stepImageDrawer)
{
 
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
      curRobot = ByteUnpack(render.image(i));
      i++;
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
  
}

}
