/*////////////////////////////////////////////////////////////////////////////////////////////////
 PNGViewer program
 This program communications with clock servers and region servers
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <tr1/memory>
#include <fstream>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <google/protobuf/message_lite.h>

#include "../common/timestep.pb.h"
#include "../common/net.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
#include "../common/messagewriter.h"
#include "../common/worldinfo.pb.h"
#include "../common/regionrender.pb.h"

#include "../common/ports.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/net.h"
#include "../common/except.h"
#include "../common/parseconf.h"
#include "../common/timestep.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"

#include "../common/helper.h"
#include <Magick++.h>
#include <gtk/gtk.h>
#include <cairo.h>

using namespace std;

struct regionConnection : helper::connection{
	RegionInfo info;

	regionConnection (int fd, RegionInfo info_) : helper::connection(fd), info(info_) {}

};

//variable declarations
TimestepUpdate timestep;
GIOChannel *ioch; //event handler
MessageType type;
size_t len;
const void *buffer;
ofstream debug;
vector<regionConnection*> regions;

void loadConfigFile(const char *configFileName, char* clockip) {
	//load the config file

	conf configuration = parseconf(configFileName);

	//clock ip address
	if (configuration.find("CLOCKIP") == configuration.end()) {
#ifdef DEBUG
		debug << "Config file is missing an entry!" << endl;
#endif
		exit(1);
	}
	strcpy(clockip, configuration["CLOCKIP"].c_str());

}

//window destruction methods
static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
	return FALSE;
}
static void destroy(GtkWidget *widget, gpointer data) {
	gtk_main_quit();
}

static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event,
		gpointer data) {
	//Test drawing function
	cairo_surface_t *image;

	image = cairo_image_surface_create_from_png("rose.png");

	cairo_t *cr;

	cr = gdk_cairo_create(widget->window);

	cairo_set_source_surface(cr, image, 10, 10);
	cairo_paint(cr);

	cairo_destroy(cr);

	return FALSE;
}

//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();

	regionConnection * regionConn;
	//get the reader for the handle
	for (vector<regionConnection*>::iterator i = regions.begin(); i != regions.end(); i++)
		if ((*i)->fd == g_io_channel_unix_get_fd(ioch)) {
			regionConn = (*i);
			break;
		}

	for (bool complete = false; !complete;)
		complete = regionConn->reader.doRead(&type, &len, &buffer);

	switch (type) {
	case MSG_REGIONRENDER: {
#ifdef DEBUG
		debug << "Received MSG_REGIONRENDER update!" << endl;
#endif
		break;
	}
	default:{
#ifdef DEBUG
		debug << "Unexpected readable socket from region! Type:" << type << "|"
				<< MSG_REGIONRENDER << endl;
#endif
	}
	}

	return TRUE;
}

//handler for clock received messages
gboolean io_clockmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();
	MessageReader *clockReader = (MessageReader*)data;

	for (bool complete = false; !complete;)
		complete = clockReader->doRead(&type, &len, &buffer);

	switch (type) {
	case MSG_REGIONINFO: {
		//we got regionserver information
		RegionInfo regioninfo;
		regioninfo.ParseFromArray(buffer, len);
#ifdef DEBUG
		debug << "Received MSG_REGIONINFO update! " << regioninfo.address()
				<< " " << regioninfo.renderport() << endl;
#endif
		//connect to the server
		struct in_addr addr;
		addr.s_addr = regioninfo.address();
		int regionFd = net::do_connect(addr, regioninfo.renderport());
		net::set_blocking(regionFd, false);

		if( regionFd<0 ){
#ifdef DEBUG
			debug << "Critical Error: Failed to connect to a region server: "<<addr.s_addr<< endl;
#endif
			exit(1);
		}
		//store the region server mapping
		regionConnection *newregion = new regionConnection(regionFd, regioninfo);
		regions.push_back(newregion);
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN, io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server: "<<addr.s_addr<< endl;
#endif
		//update knowledge of the world

		break;
	}

	default:
		cerr << "Unexpected readable message type from clock! Type:" << type << endl;
	}

	return TRUE;
}

//main method
int main(int argc, char* argv[]) {
	g_type_init();
	char clockip[40];
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	helper::Config config(argc, argv);
	const char *configFileName = (config.getArg("-c").length() == 0 ? "config"
			: config.getArg("-c").c_str());
	int clockfd;
	int windowWidth = 900, windowHeight = 900;
	debug.open("/tmp/pngviewer_debug.txt", ios::out);
#ifdef DEBUG
	debug << "Using config file: " << configFileName << endl;
#endif
	loadConfigFile(configFileName, clockip);

	//connect to the clock server
	clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
	net::set_blocking(clockfd, false);

	if( clockfd<0 ){
#ifdef DEBUG
		debug << "Critical Error: Failed to connect to the clock server: "<<clockip<< endl;
#endif
		exit(1);
	}

	MessageReader clockReader(clockfd);
#ifdef DEBUG
	debug << "Connected to Clock Server "<< clockip<< endl;
#endif
	//create the window object and init
	gtk_init(&argc, &argv);

	//link window events
	g_signal_connect(window, "expose-event",
			G_CALLBACK(on_expose_event), NULL);
	g_signal_connect (window, "delete-event",
			G_CALLBACK (delete_event), NULL);
	g_signal_connect (window, "destroy",
			G_CALLBACK (destroy), NULL);

	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage, (gpointer)&clockReader);

	//set the border width of the window.
	gtk_container_set_border_width(GTK_CONTAINER (window), 10);

	//set some window params
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), "PNGViewer");
	gtk_window_set_default_size(GTK_WINDOW(window), windowWidth, windowHeight);
	gtk_widget_set_app_paintable(window, TRUE);

	//gtk_widget_show_all(window);

	gtk_main();

	//show the window
	//gtk_widget_show(window);

	gtk_main();

	debug.close();

	return 0;
}
