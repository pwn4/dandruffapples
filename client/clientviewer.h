/*////////////////////////////////////////////////////////////////////////////////////////////////
 ClientViewer program
 //////////////////////////////////////////////////////////////////////////////////////////////////*/

#ifndef _CLIENTVIEWER_H_
#define _CLIENTVIEWER_H_

#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "../common/helper.h"

using namespace std;


struct passToThread{
	int argc;
	char** argv;
	int numberOfRobots;

	passToThread(int _argc, char** _argv, int _numberOfRobots) : argc(_argc), argv(_argv), numberOfRobots(_numberOfRobots){};
};

struct dataToHandler{
	ostream *debug;
	void* data;

	dataToHandler(ostream *_debug, void* _data) : debug(_debug), data(_data){};
};

class ClientViewer {
private:
#ifdef DEBUG
	ofstream debug;
#endif

	int* currentRobot;
	GtkBuilder *builder;

public:
	void initClientViewer(int);
	void updateViewer();

	ClientViewer(){};
	ClientViewer(int, char**, int*);
	~ClientViewer();
};

#endif
