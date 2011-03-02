/*////////////////////////////////////////////////////////////////////////////////////////////////
 ClientViewer program
 //////////////////////////////////////////////////////////////////////////////////////////////////*/

#ifndef _CLIENTVIEWER_H_
#define _CLIENTVIEWER_H_

#include <iostream>
#include <fstream>
#include <string>

#include <gtk/gtk.h>

#include "../common/helper.h"

using namespace std;

struct clientViewerShared{
	int trackRobot;
	int robotsPerClient;

	clientViewerShared(int _robotsPerClient) : trackRobot(-1), robotsPerClient(_robotsPerClient) {};
	clientViewerShared() : trackRobot(-1), robotsPerClient(0) {};
};

class ClientViewer {
private:
	GtkBuilder *builder;
#ifdef DEBUG
	ofstream debug;
#endif
	clientViewerShared shared;

public:
	void run();

	ClientViewer(int, char**, clientViewerShared);
	ClientViewer(clientViewerShared _shared) {
		ClientViewer(0, NULL, _shared);
	};
	~ClientViewer();
};

#endif
