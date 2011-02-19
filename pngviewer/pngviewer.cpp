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
#include <fstream>
#include <string>
#include <tr1/memory>
#include <map>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <stdio.h>
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
using namespace google;
using namespace protobuf;

struct regionConnection: net::connection {
	RegionInfo info;

	regionConnection(int fd, RegionInfo info_) :
		net::connection(fd), info(info_) {
	}

};

//variable declarations
/////////////////////////////////////////////////////
vector<regionConnection*> regions;
vector<GtkDrawingArea*> pngDrawingArea;
bool horizontalView = true;
GtkBuilder *builder;
//this is the region that the navigation will move the grid around
regionConnection *pivotRegion = NULL, *pivotRegionBuddy = NULL;
uint32 worldServerRows = 0, worldServerColumns = 0;

/* Position in a grid:
 * ____
 * |1|2|
 * |3|_|
 */
enum Position {
	TOP_LEFT = 0, TOP_RIGHT = 1, BOTTOM_LEFT = 2
};

#ifdef DEBUG
ofstream debug;
#endif
///////////////////////////////////////////////////////

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
	int position = TOP_LEFT;

	if (horizontalView && regions.at(regionNum) == pivotRegionBuddy)
		position = TOP_RIGHT;
	else if (!horizontalView && regions.at(regionNum) == pivotRegionBuddy)
		position = BOTTOM_LEFT;

	cairo_t *cr = gdk_cairo_create(pngDrawingArea.at(position)->widget.window);
	cairo_surface_t *image = cairo_image_surface_create_for_data((unsigned char*) render.image().c_str(), IMAGEFORMAT,
			IMAGEWIDTH, IMAGEHEIGHT, cairo_format_stride_for_width(IMAGEFORMAT, IMAGEWIDTH));

	cairo_set_source_surface(cr, image, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
}

//set whether a region with a given 'fd' will send or not send PNGs
void sendPngs(regionConnection *stoppingRegion, bool send) {
	MessageWriter writer(stoppingRegion->fd);

	SendMorePngs sendMore;
	sendMore.set_enable(send);
	writer.init(MSG_SENDMOREPNGS, sendMore);
#ifdef DEBUG
	debug << "Telling server ( " << stoppingRegion->info.address() << ":" << stoppingRegion->info.renderport()
			<< " ) to stop sending PNGs" << endl;
#endif
	for (bool complete = false; !complete;) {
		complete = writer.doWrite();
	}

}
//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();
	MessageType type;
	int len;
	const void *buffer;
	int regionNum = 0;

	//get the region number that we are receiving a PNG from
	for (vector<regionConnection*>::iterator it = regions.begin(); it != regions.end() && (*it)->fd
			!= g_io_channel_unix_get_fd(ioch); it++, regionNum++) {}

	for (bool complete = false; !complete;)
		complete = regions.at(regionNum)->reader.doRead(&type, &len, &buffer);

	//if the server is not viewed then tell it to stop sending to us
	if (regions.at(regionNum)->fd != pivotRegion->fd && (pivotRegionBuddy == NULL || regions.at(regionNum)->fd
			!= pivotRegionBuddy->fd)) {
		sendPngs(regions.at(regionNum), false);
	} else {
		switch (type) {
		case MSG_REGIONRENDER: {
			RegionRender render;
			render.ParseFromArray(buffer, len);

#ifdef DEBUG
			debug << "Received render update from server: " << regions.at(regionNum)->info.address() << ":"
					<< regions.at(regionNum)->info.renderport() << " and the timestep is # " << render.timestep()
					<< endl;
#endif
			displayPng(regionNum, render);

			break;
		}
		default: {
#ifdef DEBUG
			debug << "Unexpected readable socket from region! Type:" << type << endl;
#endif
		}
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

		//connect to the server
		struct in_addr addr;
		addr.s_addr = regioninfo.address();
		int regionFd = net::do_connect(addr, regioninfo.renderport());
		net::set_blocking(regionFd, false);

		//when the PNG viewer starts it only shows a horizontal rectangle of PNGs from region servers (0,0) and (0,1)
		if (regioninfo.draw_x() == 0 && regioninfo.draw_y() == 0) {

			gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(TOP_LEFT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegion = regions.at(regions.size() - 1);

#ifdef DEBUG
			debug << "The server in the top left is: " << regioninfo.address() << ":" << regioninfo.renderport()
					<< endl;
#endif
		} else if (regioninfo.draw_x() == 1 && regioninfo.draw_y() == 0) {

			gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegionBuddy = regions.at(regions.size() - 1);
#ifdef DEBUG
			debug << "The server in the top right is: " << regioninfo.address() << ":" << regioninfo.renderport()
					<< endl;
#endif
		} else {
			regions.push_back(new regionConnection(regionFd, regioninfo));
#ifdef DEBUG
			debug << "The server is disabled: " << regioninfo.address() << ":" << regioninfo.renderport() << endl;
#endif
		}

		//calculate the size of the world from the largest x and y coordinates that we received from the clock server
		if (regioninfo.draw_y() > worldServerRows)
			worldServerRows = regioninfo.draw_y();
		if (regioninfo.draw_x() > worldServerColumns)
			worldServerColumns = regioninfo.draw_x();

		if (regionFd < 0) {
#ifdef DEBUG
			debug << "Critical Error: Failed to connect to a region server: " << regioninfo.address() << ":"
					<< regioninfo.renderport() << endl;
#endif
			exit(1);
		}

		//create a new handler to wait for when the server sends a new PNG to us
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN, io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server: " << regioninfo.address() << ":" << regioninfo.renderport()
				<< " located at ( " << regioninfo.draw_x() << ", " << regioninfo.draw_y() << " )" << endl;
#endif
		break;
	}
	default: {
#ifdef DEBUG
		debug << "Unexpected readable message type from clock! Type:" << type << endl;
#endif
	}
	}

	return TRUE;
}

//find and set the new pivotRegion and its buddy from the new coordinates
void setNewRegionPivotAndBuddy(uint32 newPivotRegion[], uint32 newPivotRegionBuddy[]) {

#ifdef DEBUG
	debug << "Changing pivotRegion to (" << newPivotRegion[0] << "," << newPivotRegion[1]
			<< ") and pivotRegionBuddy to (" << newPivotRegionBuddy[0] << "," << newPivotRegionBuddy[1] << ")" << endl;
#endif

	for (vector<regionConnection*>::iterator it = regions.begin(); it != regions.end(); it++) {

		if ((*it)->info.draw_x() == newPivotRegion[0] && (*it)->info.draw_y() == newPivotRegion[1])
			pivotRegion = (*it);
		else if ((*it)->info.draw_x() == newPivotRegionBuddy[0] && (*it)->info.draw_y() == newPivotRegionBuddy[1])
			pivotRegionBuddy = (*it);
	}
}

//up button handler
void on_upButton_clicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked up" << endl;
#endif
	uint32 tmp1, tmp2;
	//yeah, it would be nice to use the modulo operation, but in some cases worldServerRows=0, worldServerColumns=0
	if (pivotRegion->info.draw_y() + 1 > worldServerRows)
		tmp1 = 0;
	else
		tmp1 = pivotRegion->info.draw_y() + 1;

	if (pivotRegionBuddy->info.draw_y() + 1 > worldServerRows)
		tmp2 = 0;
	else
		tmp2 = pivotRegionBuddy->info.draw_y() + 1;

	uint32 newPivotRegion[2] = { pivotRegion->info.draw_x(), tmp1 };
	uint32 newPivotRegionBuddy[2] = { pivotRegionBuddy->info.draw_x(), tmp2 };

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);
}

//down button handler
void on_downButton_clicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked down" << endl;
#endif
	uint32 tmp1, tmp2;

	if ((int) pivotRegion->info.draw_y() - 1 < 0)
		tmp1 = worldServerRows;
	else
		tmp1 = pivotRegion->info.draw_y() - 1;

	if ((int) pivotRegionBuddy->info.draw_y() - 1 < 0)
		tmp2 = worldServerRows;
	else
		tmp2 = pivotRegionBuddy->info.draw_y() - 1;

	uint32 newPivotRegion[2] = { pivotRegion->info.draw_x(), tmp1 };
	uint32 newPivotRegionBuddy[2] = { pivotRegionBuddy->info.draw_x(), tmp2 };

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);
}

//back button handler
void on_backButton_clicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked back" << endl;
#endif
	uint32 tmp1, tmp2;
	if (((int) pivotRegion->info.draw_x()) - 1 < 0)
		tmp1 = worldServerColumns;
	else
		tmp1 = pivotRegion->info.draw_x() - 1;

	if (((int) pivotRegionBuddy->info.draw_x()) - 1 < 0)
		tmp2 = worldServerColumns;
	else
		tmp2 = pivotRegionBuddy->info.draw_x() - 1;

	uint32 newPivotRegion[2] = { tmp1, pivotRegion->info.draw_y() };
	uint32 newPivotRegionBuddy[2] = { tmp2, pivotRegionBuddy->info.draw_y() };

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);

}

//forward button handler
void on_forwardButton_clicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked forward" << endl;
#endif
	uint32 tmp1, tmp2;
	//yeah, it would be nice to use the modulo operation, but in some cases worldServerRows=0, worldServerColumns=0
	if (pivotRegion->info.draw_x() + 1 > worldServerColumns)
		tmp1 = 0;
	else
		tmp1 = pivotRegion->info.draw_x() + 1;

	if (pivotRegionBuddy->info.draw_x() + 1 > worldServerColumns)
		tmp2 = 0;
	else
		tmp2 = pivotRegionBuddy->info.draw_x() + 1;

	uint32 newPivotRegion[2] = { tmp1, pivotRegion->info.draw_y() };
	uint32 newPivotRegionBuddy[2] = { tmp2, pivotRegionBuddy->info.draw_y() };

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);
}

//rotate button handler
void on_rotateButton_clicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked rotate" << endl;
#endif
	horizontalView = !horizontalView;

	uint32 newPivotRegionBuddy[2], tmp1 = pivotRegion->info.draw_x(), tmp2 = pivotRegion->info.draw_y();

	if (horizontalView) {
		gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
		gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(BOTTOM_LEFT)), 0, 0);
		if (pivotRegion->info.draw_x() + 1 > worldServerColumns)
			tmp1 = 0;
		else
			tmp1 += 1;
	} else {
		gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
		gtk_widget_set_size_request(GTK_WIDGET(pngDrawingArea.at(BOTTOM_LEFT)), 0, 0);
		if (pivotRegion->info.draw_y() + 1 > worldServerRows)
			tmp2 = 0;
		else
			tmp2 += 1;
	}

	newPivotRegionBuddy[0] = tmp1;
	newPivotRegionBuddy[1] = tmp2;

	//really rare case that happens when we have a very small world like 2x1
	//if( pivo)

#ifdef DEBUG
	debug << "Changing pivotRegionBuddy to (" << newPivotRegionBuddy[0] << "," << newPivotRegionBuddy[1] << ")" << endl;
#endif

	for (vector<regionConnection*>::iterator it = regions.begin(); it != regions.end(); it++) {
		if ((*it)->info.draw_x() == newPivotRegionBuddy[0] && (*it)->info.draw_y() == newPivotRegionBuddy[1])
			pivotRegionBuddy = (*it);
	}
}

//navigation button handler
void on_Navigation_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *navigationWindow = GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_show_all(navigationWindow);
	} else {
		gtk_widget_hide_all(navigationWindow);
	}
}

//info button handler
void on_Info_toggled(GtkWidget *widget, gpointer window) {

	GtkWidget *infoWindow = GTK_WIDGET(gtk_builder_get_object( builder, "infoWindow" ));

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_show_all(infoWindow);
	} else {
		gtk_widget_hide_all(infoWindow);
	}
}

//initializations and simple modifications for the things that will be drawn
void initializeDrawers() {
	g_type_init();
	guint rows, columns;
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton *navigation = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));
	GtkToggleToolButton *info = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ));
	GtkWidget *table = GTK_WIDGET(gtk_builder_get_object( builder, "table" ));
	GtkWidget *upButton = GTK_WIDGET(gtk_builder_get_object( builder, "up" ));
	GtkWidget *downButton = GTK_WIDGET(gtk_builder_get_object( builder, "down" ));
	GtkWidget *backButton = GTK_WIDGET(gtk_builder_get_object( builder, "back" ));
	GtkWidget *forwardButton = GTK_WIDGET(gtk_builder_get_object( builder, "forward" ));
	GtkWidget *rotateButton = GTK_WIDGET(gtk_builder_get_object( builder, "rotate" ));

	gtk_table_get_size(GTK_TABLE(table), &rows, &columns);

	//change the color of the main window's background to black
	GdkColor bgColor;
	gdk_color_parse("black", &bgColor);
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &bgColor);

	//create a grid to display the received PNGs
	for (guint i = 0; i < rows; i++) {
		for (guint j = 0; j < columns; j++) {
			GtkDrawingArea* area = GTK_DRAWING_AREA(gtk_drawing_area_new());
			gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(area), j, j + 1, i, i + 1);
			pngDrawingArea.push_back(GTK_DRAWING_AREA(area));
		}
	}

	g_signal_connect(navigation, "toggled", G_CALLBACK(on_Navigation_toggled), (gpointer) window);
	g_signal_connect(info, "toggled", G_CALLBACK(on_Info_toggled), (gpointer) window);
	g_signal_connect(upButton, "clicked", G_CALLBACK(on_upButton_clicked), (gpointer) window);
	g_signal_connect(downButton, "clicked", G_CALLBACK(on_downButton_clicked), (gpointer) window);
	g_signal_connect(backButton, "clicked", G_CALLBACK(on_backButton_clicked), (gpointer) window);
	g_signal_connect(forwardButton, "clicked", G_CALLBACK(on_forwardButton_clicked), (gpointer) window);
	g_signal_connect(rotateButton, "clicked", G_CALLBACK(on_rotateButton_clicked), (gpointer) window);

	gtk_widget_show_all(window);

	gtk_main();
}

int main(int argc, char* argv[]) {
	map<int, bool> test;

	gtk_init(&argc, &argv);

	char clockip[40];
	helper::Config config(argc, argv);

	//assume that the pngviewer.builder is in the same directory as the executable that we are running
	string builderPath(argv[0]);
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1) + "pngviewer.builder";
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	//todo: this probably does not do anything
	gtk_builder_connect_signals(builder, NULL);

	const char *configFileName = (config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
#ifdef DEBUG
	debug.open(helper::pngViewerDebugLogName.c_str(), ios::out);
	debug << "Using config file: " << configFileName << endl;
#endif
	loadConfigFile(configFileName, clockip);

	//connect to the clock server
	int clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);

	if (clockfd < 0) {
		cerr << "Critical Error: Failed to connect to the clock server: " << clockip << endl;
		exit(1);
	}
	net::set_blocking(clockfd, false);

	//handle clock messages
	MessageReader clockReader(clockfd);
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage, (gpointer) &clockReader);

#ifdef DEBUG
	debug << "Connected to Clock Server " << clockip << endl;
#endif

	initializeDrawers();

#ifdef DEBUG
	debug.close();
#endif
	g_object_unref(G_OBJECT( builder ));

	return 0;
}
