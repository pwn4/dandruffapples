#ifndef _GLOBALCONSNTATS_H_
#define _GLOBALCONSNTATS_H_


//worldviewer constants
#define IMAGEWIDTH 2500
#define IMAGEHEIGHT 2500
#define WVDRAWFACTOR 1.0
#define WVZOOMSPEED 0.2
#define WVMINZOOMED 0.1
#define WVMAXZOOMED 10
#define MINDIAMETER 1.0

//enable regionserver logging to file
//#define ENABLE_LOGGING

//printing of debug logs ( logmerger, clientviewer, worldivewer only )
//#define DEBUG
#define DEBUG_COUT

//areaengine constants
#define REGIONSIDELEN 2500
#define ROBOTDIAMETER 4
#define PUCKDIAMETER 1
#define MINELEMENTSIZE 10
#define VIEWDISTANCE 20
#define VIEWANGLE M_PI/2.0
#define MAXSPEED 4
#define MAXROTATE 2
#define HOMEDIAMETER 40
#define MINDISTANCEFROMHOME 120

//client viewer constants
#define CVDRAWFACTOR 10
#define CVZOOMSPEED 1
#define CVMINZOOMED 5
#define CVMAXZOOMED 20
//draw every DRAWTIME microseconds
#define DRAWTIME 100000

#endif
