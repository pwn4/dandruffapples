#ifndef _GLOBALCONSNTATS_H_
#define _GLOBALCONSNTATS_H_


//worldviewer constants
#define IMAGEWIDTH 625
#define IMAGEHEIGHT 625


//printing of debug logs ( logmerger, clientviewer, worldivewer only )
//#define DEBUG
//enable regionserver logging to file
//#define ENABLE_LOGGING


//areaengine constants
#define REGIONSIDELEN 2500
#define ROBOTDIAMETER 4
#define PUCKDIAMETER 1
#define MINELEMENTSIZE 25
#define VIEWDISTANCE 20
#define VIEWANGLE 360
#define MAXSPEED 4
#define MAXROTATE 2


//client viewer constants
#define DRAWFACTOR 10
#define ZOOMSPEED 1
#define MINZOOMED 5
#define MAXZOOMED 20
//draw every DRAWTIME microseconds
#define DRAWTIME 100000

#endif
