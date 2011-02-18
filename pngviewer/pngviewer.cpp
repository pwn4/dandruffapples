/*////////////////////////////////////////////////////////////////////////////////////////////////
 PNGViewer program
 This program communications with the clock server and region servers
 It gets the region server list from the clock server, connects to all the region servers,
 receives all of the PNGs every so often and displays them in a GUI for the user to watch.
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
#include <stdlib.h>

#include <google/protobuf/message_lite.h>

#include "../common/worldinfo.pb.h"
#include "../common/regionrender.pb.h"
#include "../common/ports.h"
#include "../common/messagereader.h"
#include "../common/messagewriter.h"
#include "../common/net.h"
#include "../common/types.h"
#include "../common/except.h"
#include "../common/parseconf.h"
#include "../common/timestep.pb.h"

#include "../common/helper.h"
#include "../common/imageconstants.h"
#include <gtk/gtk.h>
#include <cairo.h>

using namespace std;

struct regionConnection: net::connection {
	RegionInfo info;

	regionConnection(int fd, RegionInfo info_) :
		net::connection(fd), info(info_) {
	}

};

struct region {
	regionConnection regionConn;
	bool viewed;
	int showInRow;
	int showInColumn;

	region(int fd, RegionInfo info, bool viewed_) :
		regionConn(fd, info), viewed(viewed_) {
	}
};

//variable declarations
ofstream debug;
vector<region*> regions;
vector<GtkDrawingArea*> pngDrawingArea;
GtkBuilder *builder;

//load the config file
void loadConfigFile(const char *configFileName, char* clockip) {
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

//display the png that we received from a region server in its pngDrawingArea space
void displayPng(int regionNum, RegionRender render) {

	//temporary: Don't crash the program if we have more region servers
	//sending PNGs to us than we can draw at once
	if(!regions.at(regionNum)->viewed)
		return;

	//temporary: inefficient calling this here all the time
	gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(regionNum)),
			IMAGEWIDTH, IMAGEHEIGHT);

	cairo_t *cr = gdk_cairo_create(pngDrawingArea.at(regionNum)->widget.window);
	cairo_surface_t *image =
			cairo_image_surface_create_for_data(
					(unsigned char*) render.image().c_str(), IMAGEFORMAT,
					IMAGEWIDTH, IMAGEHEIGHT, cairo_format_stride_for_width(
							IMAGEFORMAT, IMAGEWIDTH));

	cairo_set_source_surface(cr, image, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
}

//go through all the viewable regions and map them to a specific spot on the grid
void mapRegionsToDraw()
{
}

//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();
	MessageType type;
	int len;
	const void *buffer;
	int regionNum = 0;

	//get the region number that we are receiving a PNG from
	for (vector<region*>::iterator it = regions.begin(); it != regions.end()
			&& (*it)->regionConn.fd != g_io_channel_unix_get_fd(ioch); it++, regionNum++) {
	}

	for (bool complete = false; !complete;)
		complete = regions.at(regionNum)->regionConn.reader.doRead(&type, &len,
				&buffer);

	switch (type) {
	case MSG_REGIONRENDER: {
		RegionRender render;
		render.ParseFromArray(buffer, len);

#ifdef DEBUG
		debug << "Received MSG_REGIONRENDER update and the timestep is # "
				<< render.timestep() << endl;
#endif
		displayPng(regionNum, render);

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
	MessageType type;
	int len;
	const void *buffer;

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

		//store the region server mapping
		regions.push_back(new region(regionFd, regioninfo, true));

		if (regions.size() > PNGVIEWER_MAX_VIEWS_ROWS
				* PNGVIEWER_MAX_VIEWS_COLUMNS) {

			MessageWriter writer(regionFd);
			regions.at(regions.size() - 1)->viewed = false;
			SendMorePngs sendMore;
			sendMore.set_enable(false);
			writer.init(MSG_SENDMOREPNGS, sendMore);

			for (bool complete = false; !complete;) {
				complete = writer.doWrite();
			}

#ifdef DEBUG
		debug << "Too many region servers are trying to send. Disabling:  " << regioninfo.address()
				<< " " << regioninfo.renderport() << endl;
#endif
		}
		else
			mapRegionsToDraw();

		if (regionFd < 0) {
#ifdef DEBUG
			debug << "Critical Error: Failed to connect to a region server: "
					<< addr.s_addr << endl;
#endif
			exit(1);
		}

		//create a new handler to wait for when the server sends a new PNG to us
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN,
				io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server: " << addr.s_addr << " at ( "
				<< regioninfo.draw_x() << ", " << regioninfo.draw_y() << " )"
				<< endl;
#endif

		break;
	}

	default: {
#ifdef DEBUG
		debug << "Unexpected readable message type from clock! Type:" << type
				<< endl;
#endif
	}
	}

	return TRUE;
}

//navigation button handler
void on_Navigation_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *navigationWindow =
			GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_show_all(navigationWindow);
	} else {
		gtk_widget_hide_all(navigationWindow);
	}
}

//properties button handler
void on_Properties_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *propertiesWindow =
			GTK_WIDGET(gtk_builder_get_object( builder, "propertiesWindow" ));

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_show_all(propertiesWindow);
	} else {
		gtk_widget_hide_all(propertiesWindow);
	}
}

//initializations and simple modifications for the things that will be drawn
void initializeDrawers() {
	g_type_init();

	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton
			*navigation =
					GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));
	GtkToggleToolButton
			*properties =
					GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Properties" ));
	GtkWidget *table = GTK_WIDGET(gtk_builder_get_object( builder, "table" ));
	gtk_table_resize(GTK_TABLE(table), PNGVIEWER_MAX_VIEWS_ROWS,
			PNGVIEWER_MAX_VIEWS_COLUMNS);

	//change the color of the main window's background to black
	GdkColor bgColor;
	bgColor.red = 0;
	bgColor.green = 0;
	bgColor.blue = 0;
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &bgColor);

	//create a grid to display the received PNGs
	for (int i = 0; i < PNGVIEWER_MAX_VIEWS_ROWS; i++) {
		for (int j = 0; j < PNGVIEWER_MAX_VIEWS_COLUMNS; j++) {
			GtkDrawingArea* area = GTK_DRAWING_AREA(gtk_drawing_area_new());
			gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(area), j, j
					+ 1, i, i + 1);
			pngDrawingArea.push_back(GTK_DRAWING_AREA(area));
		}
	}

	g_signal_connect(navigation, "toggled", G_CALLBACK(on_Navigation_toggled), (gpointer) window);
	g_signal_connect(properties, "toggled", G_CALLBACK(on_Properties_toggled), (gpointer) window);

	gtk_widget_show_all(window);

	gtk_main();
}

int main(int argc, char* argv[]) {
	gtk_init(&argc, &argv);

	char clockip[40];
	helper::Config config(argc, argv);

	//assume that the pngviewer.builder is in the same directory as the executable that we are running
	string builderPath(argv[0]);
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1)
			+ "pngviewer.builder";
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	//todo: this probably does not do anything
	gtk_builder_connect_signals(builder, NULL);

	const char *configFileName = (config.getArg("-c").length() == 0 ? "config"
			: config.getArg("-c").c_str());
	debug.open("/tmp/pngviewer_debug.txt", ios::out);
#ifdef DEBUG
	debug << "Using config file: " << configFileName << endl;
#endif
	loadConfigFile(configFileName, clockip);

	//connect to the clock server
	int clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
	net::set_blocking(clockfd, false);

	//handle clock messages
	MessageReader clockReader(clockfd);
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage,
			(gpointer) &clockReader);

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

	initializeDrawers();

	debug.close();
	g_object_unref(G_OBJECT( builder ));

	return 0;
}
