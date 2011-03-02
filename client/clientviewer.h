#ifndef _CLIENTVIEWER_H_
#define _CLIENTVIEWER_H_

#include <iostream>
#include <fstream>
#include <string>

#include <gtk/gtk.h>
#include <cairo.h>
#include <glib.h>

#include "../common/helper.h"

using namespace std;

class ClientViewer {
private:
	GtkBuilder *builder;
#ifdef DEBUG
	ofstream debug;
#endif

	void run();

public:
	ClientViewer(int argc, char* argv[]);
	ClientViewer() {
		ClientViewer(0, NULL);
	}
	;
	~ClientViewer();
};

#endif
