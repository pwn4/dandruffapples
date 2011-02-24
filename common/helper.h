#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>
#include <netinet/in.h>
#include "imageconstants.h"
#include <cairo.h>
#include "regionrender.pb.h"

using namespace std;

//comment to disable generation of debug for certain programs
#define DEBUG
//#define ENABLE_LOGGING

namespace helper {

string toString(int);
string getNewName(string);

class Config {
public:
	Config(int, char**);
	string getArg(string);
private:
	map<string, string> parsedConfig;

};

struct TwoInt{
  int one, two;
  
  TwoInt(int first, int second) : one(first), two(second) {}
};

struct ColorObject{
  double r, g, b;
  
  ColorObject(double newr, double newg, double newb) : r(newr), g(newg), b(newb) {}
};

ColorObject colorFromTeam(int teamId);
void UnpackImage(RegionRender render, cairo_t *stepImageDrawer);
unsigned int BytePack(int a, int b);
TwoInt ByteUnpack(unsigned int data);

const string defaultLogName = "antix_log";
const string worldViewerDebugLogName="/tmp/worldviewer_debug.txt";
}

#endif
