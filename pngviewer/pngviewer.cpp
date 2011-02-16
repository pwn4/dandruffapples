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

struct regionConnection: net::connection {
	RegionInfo info;

	regionConnection(int fd, RegionInfo info_) :
		net::connection(fd), info(info_) {
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
GtkBuilder *builder;

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

		//Image image;
		//image.read(blob);

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

		break;
	}

	default:
		cerr << "Unexpected readable message type from clock! Type:" << type
				<< endl;
	}

	return TRUE;
}

//navigation button handler
G_MODULE_EXPORT void on_Navigation_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *navigation = GTK_WIDGET(gtk_builder_get_object( builder, "navigation" ));
	gtk_widget_show_all(navigation);
}

//properties button handler
G_MODULE_EXPORT void on_Properties_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *properties = GTK_WIDGET(gtk_builder_get_object( builder, "properties" ));
	gtk_widget_show_all(properties);
}


void drawer(int argc, char* argv[], int clockfd) {
	g_type_init();

	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));

	MessageReader clockReader(clockfd);

	//adder handler for the clock
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage,
			(gpointer) &clockReader);

	g_object_unref( G_OBJECT( builder ) );
	gtk_widget_show_all(window);

	gtk_main();
}

//main method
int main(int argc, char* argv[]) {
	gtk_init(&argc, &argv);
	builder = gtk_builder_new();
	gtk_builder_add_from_file( builder, "pngviewer.builder", NULL );
	gtk_builder_connect_signals(builder, NULL);
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

	drawer(argc, argv, clockfd);

	debug.close();

	return 0;
}
