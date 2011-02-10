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
using namespace Magick;

struct regionConnection: helper::connection {
	RegionInfo info;

	regionConnection(int fd, RegionInfo info_) :
		helper::connection(fd), info(info_) {
	}

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

//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();

	regionConnection * regionConn;
	//get the reader for the handle
	for (vector<regionConnection*>::iterator i = regions.begin(); i
			!= regions.end(); i++)
		if ((*i)->fd == g_io_channel_unix_get_fd(ioch)) {
			regionConn = (*i);
			break;
		}

	for (bool complete = false; !complete;)
		complete = regionConn->reader.doRead(&type, &len, &buffer);

	switch (type) {
	case MSG_REGIONRENDER: {
		RegionRender render;
		render.ParseFromArray(buffer, len);
		Blob blob((void*) render.image().c_str(), render.image().length());

#ifdef DEBUG
		debug << "Received MSG_REGIONRENDER update and the timestep is # "
				<< render.timestep() << endl;
#endif

		Image image;
		image.read(blob);
		image.write("/tmp/tmp.png");

		break;
	}
	default: {
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
	MessageReader *clockReader = (MessageReader*) data;

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

		if (regionFd < 0) {
#ifdef DEBUG
			debug << "Critical Error: Failed to connect to a region server: "
					<< addr.s_addr << endl;
#endif
			exit(1);
		}
		//store the region server mapping
		regionConnection *newregion =
				new regionConnection(regionFd, regioninfo);
		regions.push_back(newregion);
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN,
				io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server: " << addr.s_addr << endl;
#endif
		//update knowledge of the world

		break;
	}

	default:
		cerr << "Unexpected readable message type from clock! Type:" << type
				<< endl;
	}

	return TRUE;
}

//window destruction methods
static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
	return FALSE;
}
static void destroy(GtkWidget *widget, gpointer data) {
	gtk_main_quit();
}

void show_navigation(GtkWidget *widget, gpointer window) {

	GtkWidget *navigation = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *frame = gtk_frame_new (NULL);
	GtkWidget *tableNavigation = gtk_table_new(11,11,false);
	GtkWidget *upArrow = gtk_button_new_with_label("^");
	GtkWidget *downArrow = gtk_button_new_with_label("v");
	GtkWidget *leftArrow = gtk_button_new_with_label("<-");
	GtkWidget *rightArrow = gtk_button_new_with_label("->");

	//set some window params
	gtk_container_set_border_width(GTK_CONTAINER (navigation), 10);
	gtk_window_set_position(GTK_WINDOW(navigation), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(navigation), "Navigation");
	int width,height;
	gtk_window_get_size((GtkWindow*)window, &width,&height);
	gtk_window_set_default_size(GTK_WINDOW(navigation), width/6, height/3);
	gtk_window_set_opacity(GTK_WINDOW(navigation),0.1 );
	//set some frame params
	gtk_frame_set_label(GTK_FRAME (frame), "Navigation Frame");
	gtk_frame_set_label_align(GTK_FRAME (frame), 1.0, 0.0);
	gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_ETCHED_OUT);

	//add buttons into the table
	gtk_table_attach_defaults(GTK_TABLE(tableNavigation), frame, 0, 11, 0, 8 );
	gtk_table_attach_defaults(GTK_TABLE(tableNavigation), upArrow, 5, 6, 8, 9 );
	gtk_table_attach_defaults(GTK_TABLE(tableNavigation), downArrow, 5, 6, 10, 11 );
	gtk_table_attach_defaults(GTK_TABLE(tableNavigation), leftArrow, 4, 5, 9, 10 );
	gtk_table_attach_defaults(GTK_TABLE(tableNavigation), rightArrow, 6, 7, 9, 10 );

	//add the box container to the dialog
	gtk_container_add(GTK_CONTAINER(navigation), tableNavigation);

	g_signal_connect (navigation, "delete-event",
			G_CALLBACK (delete_event), NULL);
	g_signal_connect (navigation, "destroy",
			G_CALLBACK (destroy), NULL);

	gtk_widget_show_all(navigation);

	gtk_main();
}

void drawer(int argc, char* argv[], int clockfd, ofstream &debug) {
	g_type_init();

	MessageReader clockReader(clockfd);

	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *toolbar = gtk_toolbar_new();
	GtkToolItem *navigation = gtk_tool_button_new_from_stock(GTK_STOCK_ABOUT);
	GtkToolItem *properties = gtk_tool_button_new_from_stock(
			GTK_STOCK_PROPERTIES);
	GtkToolItem *exit = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
	GtkToolItem *sep = gtk_separator_tool_item_new();

	//modify window params
	gtk_container_set_border_width(GTK_CONTAINER (window), 10);
	gtk_window_set_title(GTK_WINDOW(window), "PNG Viewer");
	gtk_window_set_default_size (GTK_WINDOW(window),1000,1000);
	gtk_window_maximize (GTK_WINDOW(window));

	//modify toolbar params
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 1);

	//add items to the toolbar
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(navigation), "Navigation");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), navigation, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), properties, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), sep, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), exit, -1);

	//add toolbar to the box container
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 5);

	//add the box container to the window
	gtk_container_add(GTK_CONTAINER(window), vbox);

	//adder handler for the clock
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage,
			(gpointer) &clockReader);

	//create the window object and init
	gtk_init(&argc, &argv);

	g_signal_connect(G_OBJECT(exit), "clicked",
			G_CALLBACK(destroy), NULL);
	g_signal_connect(G_OBJECT(navigation), "clicked",
			G_CALLBACK(show_navigation), (gpointer) window);
	g_signal_connect (window, "delete-event",
			G_CALLBACK (delete_event), NULL);
	g_signal_connect (window, "destroy",
			G_CALLBACK (destroy), NULL);

	//gtk_widget_set_app_paintable(window, TRUE);

	gtk_widget_show_all(window);

	gtk_main();
}

//main method
int main(int argc, char* argv[]) {
	char clockip[40];
	helper::Config config(argc, argv);
	const char *configFileName = (config.getArg("-c").length() == 0 ? "config"
			: config.getArg("-c").c_str());
	int clockfd;
	debug.open("/tmp/pngviewer_debug.txt", ios::out);
#ifdef DEBUG
	debug << "Using config file: " << configFileName << endl;
#endif
	loadConfigFile(configFileName, clockip);

	//connect to the clock server
	clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
	net::set_blocking(clockfd, false);

	if (clockfd < 0) {
#ifdef DEBUG
		debug << "Critical Error: Failed to connect to the clock server: "
				<< clockip << endl;
#endif
		exit(1);
	}

#ifdef DEBUG
	debug << "Connected to Clock Server " << clockip << endl;
#endif

	drawer(argc, argv, clockfd, debug);

	debug.close();

	return 0;
}
