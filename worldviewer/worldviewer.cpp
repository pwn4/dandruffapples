/*////////////////////////////////////////////////////////////////////////////////////////////////
 WorldViewer program
 This program communicates with the clock server and region servers
 It gets the region server list from the clock server, connects to all the region servers,
 receives all of the robot data every so often and displays them in a GUI for the user to watch.
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <string>
#include <map>
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
#include "../common/globalconstants.h"
#include "drawer.h"

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

struct ViewedRegion {
	regionConnection *connection;
	int regionNum;
};

//define the size of the table
const guint tableWorldViewRows = 2, tableWorldViewColumns = 2;
#define DRAWINGORIGINS 4

//Variable declarations
/////////////////////////////////////////////////////
vector<regionConnection*> regions;
vector<GtkDrawingArea*> worldDrawingArea;
GtkBuilder *builder;
bool runForTheFirstTime = true;
uint32 worldServerRows = 0, worldServerColumns = 0;
map<int, map<int, GtkDrawingArea*> > worldGrid;
RegionRender renderDraw[DRAWINGORIGINS];
bool draw[] = { false, false, false, false };

//Peter's SSH stuff
int tunnelPort = -1;
int lastTunnelPort = 12345;
string userAddon;
string sshkeyloc;
string sshpid;
string sshsock;

//this is the region that the navigation will move the grid around
ViewedRegion viewedRegion[DRAWINGORIGINS];

//used to draw the homes
WorldInfo worldInfo;
map<int, unsigned int> fdToRegionId;

//used only for calculating images per second for each server
int timeCache, lastSecond[] = { 0, 0, 0, 0 }, benchMessages[] = { 0, 0, 0, 0 };

//read in from the config
double robotAlpha = 1.0;

//cache this, so we don't have to retrieve it many times
GtkToggleToolButton *InfoToolButton;

//used for zooming in and out

float drawFactor = WVDRAWFACTOR;
/* Position in a grid:
 * ____
 * |0|1|
 * |2|3|
 */
enum Position {
	TOP_LEFT = 0, TOP_RIGHT = 1, BOTTOM_LEFT = 2, BOTTOM_RIGHT = 3
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

	if (configuration.find("ROBOTALPHA") != configuration.end())
		robotAlpha = atof(configuration["ROBOTALPHA"].c_str());
}

//display the worldView that we received from a region server in its worldDrawingArea space
void displayWorldView(int regionNum, RegionRender render, int viewedRegionNum) {

	renderDraw[viewedRegionNum] = render;
	draw[viewedRegionNum] = true;
	gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(viewedRegionNum)));

}

//set whether a region with a given 'fd' will send or not send world views
void sendWorldViews(int fd, bool send) {
	MessageWriter writer(fd);

	SendMoreWorldViews sendMore;
	sendMore.set_enable(send);
	writer.init(MSG_SENDMOREWORLDVIEWS, sendMore);
#ifdef DEBUG
	debug << "Telling server with fd = " << fd << " to set the sending of world views to " << send << endl;
#endif
	for (bool complete = false; !complete;) {
		complete = writer.doWrite();
	}
}

//update the fields in the worldGrid that we are viewing with a color
void updateWorldGrid(string color) {
	GdkColor fgColor;
	gdk_color_parse(color.c_str(), &fgColor);

	for (int i = TOP_LEFT; i <= BOTTOM_RIGHT; i++) {
		gtk_widget_modify_bg(
				GTK_WIDGET(worldGrid[viewedRegion[i].connection->info.draw_x()][viewedRegion[i].connection->info.draw_y()]),
				GTK_STATE_NORMAL, &fgColor);
	}
}

//update the info window
void updateInfoWindow() {
	const string frameNumName = "frame_frameNum", frameserverAddName = "frame_serverAdd",
			frameserverLocName = "frame_serverLoc";
	string tmp, position;

	for (int frame = 1; frame <= DRAWINGORIGINS; frame++) {
		GtkLabel
				*frameNum = GTK_LABEL(gtk_builder_get_object( builder, (frameNumName+helper::toString(frame)).c_str() ));
		GtkLabel
				*frameserverAdd = GTK_LABEL(gtk_builder_get_object( builder, (frameserverAddName+helper::toString(frame)).c_str() ));
		GtkLabel
				*frameserverLoc = GTK_LABEL(gtk_builder_get_object( builder, (frameserverLocName+helper::toString(frame)).c_str() ));

		if (frame == 1)
			position = "top-left";
		else if (frame == 2)
			position = "top-right";
		else if (frame == 3)
			position = "bottom-left";
		else
			position = "bottom-right";

		tmp = "Frame positioned in the " + position + " corner";
		gtk_label_set_text(frameNum, tmp.c_str());

		tmp = helper::toString(viewedRegion[frame - 1].connection->info.address()) + ":" + helper::toString(
				viewedRegion[frame - 1].connection->info.renderport()) + " connected on fd = " + helper::toString(
				viewedRegion[frame - 1].connection->fd);
		gtk_label_set_text(frameserverAdd, tmp.c_str());

		tmp = "Server located at: (" + helper::toString(viewedRegion[frame - 1].connection->info.draw_x()) + ", "
				+ helper::toString(viewedRegion[frame - 1].connection->info.draw_y()) + " )";
		gtk_label_set_text(frameserverLoc, tmp.c_str());
	}
}

//enable the info/navigation buttons found in the main window's toolbar and do some work to initialize them
void initializeToolbarButtons() {
	if (!gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Fullscreen" )))) {
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Navigation" )), true);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Info" )), true);
	}

	int navigationWindowWidth, navigationWindowLength;
	GtkWidget *navigationWindow = GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));
	GtkWidget *worldGridTable = GTK_WIDGET(gtk_builder_get_object( builder, "worldGrid" ));
	gtk_table_resize(GTK_TABLE(worldGridTable), worldServerRows, worldServerColumns);

	gtk_window_get_default_size(GTK_WINDOW(navigationWindow), &navigationWindowWidth, &navigationWindowLength);

	for (guint i = 0; i < worldServerRows; i++) {
		for (guint j = 0; j < worldServerColumns; j++) {
			GtkDrawingArea* area = GTK_DRAWING_AREA(gtk_drawing_area_new());
			gtk_widget_set_size_request(GTK_WIDGET(area), navigationWindowWidth / worldServerColumns,
					navigationWindowLength / worldServerRows);

			gtk_table_attach_defaults(GTK_TABLE(worldGridTable), GTK_WIDGET(area), j, j + 1, i, i + 1);

			worldGrid[j][i] = area;
		}
	}

	updateInfoWindow();
	updateWorldGrid("light green");
}

//updates the number of messages found in the Info window
void benchmarkMessages(regionConnection *region, int viewedRegionNum) {
	timeCache = time(NULL);
	benchMessages[viewedRegionNum]++;

	//check if its time to output
	if (timeCache > lastSecond[viewedRegionNum]) {
		string tmp = "frame_serverId" + helper::toString(viewedRegionNum + 1);

		GtkLabel *frameserverId = GTK_LABEL(gtk_builder_get_object( builder, tmp.c_str() ));
		tmp = "Server: " + helper::toString(viewedRegion[viewedRegionNum].connection->info.id()) + " sending "
				+ helper::toString(benchMessages[viewedRegionNum]) + " images/second";
		gtk_label_set_text(frameserverId, tmp.c_str());

		benchMessages[viewedRegionNum] = 0;
		lastSecond[viewedRegionNum] = timeCache;
	}
}

//handler for region received messages
gboolean regionMessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	MessageType type;
	const void *buffer;
	int regionNum = 0, len, viewedRegionNum = -1;

	//get the region number that we are receiving a world view from
	for (vector<regionConnection*>::const_iterator it = regions.begin();
			it != regions.end() && (*it)->fd != g_io_channel_unix_get_fd(ioch); it++, regionNum++) {
	}

	for (bool complete = false; !complete;)
		complete = regions.at(regionNum)->reader.doRead(&type, &len, &buffer);

	//things that have to be done only when we receive the first world view
	if (runForTheFirstTime) {
		runForTheFirstTime = false;

		//our rows and columns span from 0, so their size is +1
		worldServerRows++;
		worldServerColumns++;

		if (worldServerRows * worldServerColumns < DRAWINGORIGINS)
			throw SystemError("The World Viewer will not work with less that four region servers");

		initializeToolbarButtons();
	}

	switch (type) {
	case MSG_REGIONVIEW: {
		RegionRender render;
		render.ParseFromArray(buffer, len);

		//we got an update from a region, but how what drawing area is responsible for that region?
		for (int i = TOP_LEFT; i < DRAWINGORIGINS; i++) {
			if (viewedRegion[i].regionNum == regionNum) {
				viewedRegionNum = i;
				break;
			}
		}

		//there is a delay between telling a region to stop sending and when it actually STOPS sending
		if (viewedRegionNum == -1) {
#ifdef DEBUG
			debug << "Received AN UNWANTED render update from server with fd=" << regions.at(regionNum)->fd << " skipping."<< endl;
#endif

			return TRUE;
		}

#ifdef DEBUG
		debug << "Received render update from server with fd=" << regions.at(regionNum)->fd << " and the timestep is # "
		<< render.timestep() << endl;
#endif
		if (gtk_toggle_tool_button_get_active(InfoToolButton))
			benchmarkMessages(regions.at(regionNum), viewedRegionNum);

		displayWorldView(regionNum, render, viewedRegionNum);

		break;
	}

	default: {
#ifdef DEBUG
		debug << "Unexpected readable socket from region! Type:" << type << endl;
#endif
		break;
	}
	}

	return TRUE;
}

//handler for clock received messages
gboolean clockMessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
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

		int regionFd;
		//setup an ssh tunnel first
		if (tunnelPort != -1) {
			char * stringAddr = inet_ntoa(addr);
			stringstream tunnelcmd;

			tunnelcmd << sshkeyloc << "ssh -f -N -p" << tunnelPort << " -L " << lastTunnelPort << ":127.0.0.1:"
					<< regioninfo.renderport() << " " << userAddon << stringAddr;

			//setup the tunnel
			if (system(tunnelcmd.str().c_str()) != 0)
				throw runtime_error("Unable to establish ssh connection");

			addr.s_addr = inet_addr("127.0.0.1");
			regionFd = net::do_connect(addr, lastTunnelPort);
			lastTunnelPort++;
			//cout << tunnelcmd.str() << endl;
		} else
			regionFd = net::do_connect(addr, regioninfo.renderport());

		net::set_blocking(regionFd, false);
		fdToRegionId[regionFd] = regioninfo.id();

		//when the world viewer starts it only shows a horizontal rectangle of world views from region servers (0,0) and (0,1)
		if (regioninfo.draw_x() == 0 && regioninfo.draw_y() == 0) {
			regions.push_back(new regionConnection(regionFd, regioninfo));
			viewedRegion[TOP_LEFT].regionNum = regions.size() - 1;
			viewedRegion[TOP_LEFT].connection = regions.at(viewedRegion[TOP_LEFT].regionNum);
			sendWorldViews(viewedRegion[TOP_LEFT].connection->fd, true);
		} else if (regioninfo.draw_x() == 1 && regioninfo.draw_y() == 0) {
			regions.push_back(new regionConnection(regionFd, regioninfo));
			viewedRegion[TOP_RIGHT].regionNum = regions.size() - 1;
			viewedRegion[TOP_RIGHT].connection = regions.at(viewedRegion[TOP_RIGHT].regionNum);
			sendWorldViews(viewedRegion[TOP_RIGHT].connection->fd, true);
		} else if (regioninfo.draw_x() == 0 && regioninfo.draw_y() == 1) {
			regions.push_back(new regionConnection(regionFd, regioninfo));
			viewedRegion[BOTTOM_LEFT].regionNum = regions.size() - 1;
			viewedRegion[BOTTOM_LEFT].connection = regions.at(viewedRegion[BOTTOM_LEFT].regionNum);
			sendWorldViews(viewedRegion[BOTTOM_LEFT].connection->fd, true);
		} else if (regioninfo.draw_x() == 1 && regioninfo.draw_y() == 1) {
			regions.push_back(new regionConnection(regionFd, regioninfo));
			viewedRegion[BOTTOM_RIGHT].regionNum = regions.size() - 1;
			viewedRegion[BOTTOM_RIGHT].connection = regions.at(viewedRegion[BOTTOM_RIGHT].regionNum);
			sendWorldViews(viewedRegion[BOTTOM_RIGHT].connection->fd, true);
		} else {
			regions.push_back(new regionConnection(regionFd, regioninfo));
#ifdef DEBUG
			debug << "The server is disabled: fd= " << regionFd << endl;
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

		//create a new handler to wait for when the server sends a new world view to us
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN, regionMessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server fd=" << regionFd << " located at ( " << regioninfo.draw_x() << ", "
		<< regioninfo.draw_y() << " )" << endl;
#endif
		break;
	}
	case MSG_WORLDINFO: {
		worldInfo.ParseFromArray(buffer, len);

		break;
	}
	default: {
#ifdef DEBUG
		debug << "Unexpected readable message type from clock! Type:" << type << endl;
#endif
		break;
	}
	}

	return TRUE;
}

//call when we change the draw factor. Sends the new draw factor to all the currently viewed regions and resizes the drawing areas.
void updateDrawFactor() {
	for (int i = TOP_LEFT; i <= BOTTOM_RIGHT; i++) {
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(i)), IMAGEWIDTH * drawFactor,
				IMAGEHEIGHT * drawFactor);

	}

#ifdef DEBUG
	debug<<"Updating drawFactor to  "<<helper::toString(drawFactor)<<endl;
#endif
}

//find and new regions to be drawn
void setNewViewedRegion(int newViewed[][2]) {

#ifdef DEBUG
	debug << "Changing viewedRegion[TOP_LEFT] to (" << newViewed[0][0] << "," << newViewed[0][1]
	<< "), changing viewedRegion[TOP_RIGHT] to (" << newViewed[1][0] << "," << newViewed[1][1]
	<< "), changing viewedRegion[BOTTOM_LEFT] to (" << newViewed[2][0] << "," << newViewed[2][1]
	<< "), changing viewedRegion[BOTTOM_RIGHT] to (" << newViewed[3][0] << "," << newViewed[3][1]<<")"<<endl;
#endif

	//update the world grid to have no currently viewed regions
	updateWorldGrid("white");

	for (int i = 0; i < (int) regions.size(); i++) {
		if ((int) regions.at(i)->info.draw_x() == newViewed[TOP_LEFT][0] && (int) regions.at(i)->info.draw_y()
				== newViewed[TOP_LEFT][1]) {
			viewedRegion[TOP_LEFT].connection = regions.at(i);
			viewedRegion[TOP_LEFT].regionNum = i;
		} else if ((int) regions.at(i)->info.draw_x() == newViewed[TOP_RIGHT][0] && (int) regions.at(i)->info.draw_y()
				== newViewed[TOP_RIGHT][1]) {
			viewedRegion[TOP_RIGHT].connection = regions.at(i);
			viewedRegion[TOP_RIGHT].regionNum = i;
		} else if ((int) regions.at(i)->info.draw_x() == newViewed[BOTTOM_LEFT][0] &&
				(int) regions.at(i)->info.draw_y() == newViewed[BOTTOM_LEFT][1]) {
			viewedRegion[BOTTOM_LEFT].connection = regions.at(i);
			viewedRegion[BOTTOM_LEFT].regionNum = i;
		} else if ((int) regions.at(i)->info.draw_x() == newViewed[BOTTOM_RIGHT][0] &&
				(int) regions.at(i)->info.draw_y() == newViewed[BOTTOM_RIGHT][1]) {
			viewedRegion[BOTTOM_RIGHT].connection = regions.at(i);
			viewedRegion[BOTTOM_RIGHT].regionNum = i;
		}
	}

	updateWorldGrid("light green");

	//only update the info window if the its button is toggled
	if (gtk_toggle_tool_button_get_active(InfoToolButton))
		updateInfoWindow();

	updateDrawFactor();
}

//up button handler
void onDownButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked down" << endl;
#endif
	int newViewed[DRAWINGORIGINS][2];

	for (int i = TOP_LEFT; i < DRAWINGORIGINS; i++) {
		newViewed[i][0] = viewedRegion[i].connection->info.draw_x();
		newViewed[i][1] = (viewedRegion[i].connection->info.draw_y() + 1) % worldServerRows;
	}

	//disable the sending of world views from regions that we are moving away from
	sendWorldViews(viewedRegion[TOP_LEFT].connection->fd, false);
	sendWorldViews(viewedRegion[TOP_RIGHT].connection->fd, false);

	//set the new viewedRegions
	setNewViewedRegion(newViewed);

	//enable the sending of world views from regions that we are moving to
	sendWorldViews(viewedRegion[BOTTOM_LEFT].connection->fd, true);
	sendWorldViews(viewedRegion[BOTTOM_RIGHT].connection->fd, true);
}

//down button handler
void onUpButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked up" << endl;
#endif
	int newViewed[DRAWINGORIGINS][2];

	for (int i = TOP_LEFT; i < DRAWINGORIGINS; i++) {
		newViewed[i][0] = viewedRegion[i].connection->info.draw_x();
		newViewed[i][1] = viewedRegion[i].connection->info.draw_y() - 1;

		if (newViewed[i][1] == -1)
			newViewed[i][1] = worldServerRows - 1;
	}

	//disable the sending of world views from regions that we are moving away from
	sendWorldViews(viewedRegion[BOTTOM_LEFT].connection->fd, false);
	sendWorldViews(viewedRegion[BOTTOM_RIGHT].connection->fd, false);

	//set the new viewedRegions
	setNewViewedRegion(newViewed);

	//enable the sending of world views from regions that we are moving to
	sendWorldViews(viewedRegion[TOP_LEFT].connection->fd, true);
	sendWorldViews(viewedRegion[TOP_RIGHT].connection->fd, true);
}

//back button handler
void onBackButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked back" << endl;
#endif
	int newViewed[DRAWINGORIGINS][2];

	for (int i = TOP_LEFT; i < DRAWINGORIGINS; i++) {
		newViewed[i][0] = viewedRegion[i].connection->info.draw_x() - 1;
		newViewed[i][1] = viewedRegion[i].connection->info.draw_y();

		if (newViewed[i][0] == -1)
			newViewed[i][0] = worldServerColumns - 1;
	}

	//disable the sending of world views from regions that we are moving away from
	sendWorldViews(viewedRegion[TOP_RIGHT].connection->fd, false);
	sendWorldViews(viewedRegion[BOTTOM_RIGHT].connection->fd, false);

	//set the new viewedRegions
	setNewViewedRegion(newViewed);

	//enable the sending of world views from regions that we are moving to
	sendWorldViews(viewedRegion[TOP_LEFT].connection->fd, true);
	sendWorldViews(viewedRegion[BOTTOM_LEFT].connection->fd, true);
}

//forward button handler
void onForwardButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked forward" << endl;
#endif
	int newViewed[DRAWINGORIGINS][2];

	for (int i = TOP_LEFT; i < DRAWINGORIGINS; i++) {
		newViewed[i][0] = (viewedRegion[i].connection->info.draw_x() + 1) % worldServerColumns;
		newViewed[i][1] = viewedRegion[i].connection->info.draw_y();
	}

	//disable the sending of world views from regions that we are moving away from
	sendWorldViews(viewedRegion[TOP_LEFT].connection->fd, false);
	sendWorldViews(viewedRegion[BOTTOM_LEFT].connection->fd, false);

	//set the new viewedRegions
	setNewViewedRegion(newViewed);

	//enable the sending of world views from regions that we are moving to
	sendWorldViews(viewedRegion[TOP_RIGHT].connection->fd, true);
	sendWorldViews(viewedRegion[BOTTOM_RIGHT].connection->fd, true);
}

//window destruction methods for the info and navigation windows
static void destroy(GtkWidget *window, gpointer widget) {
	gtk_widget_hide_all(GTK_WIDGET(window));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(widget), FALSE);
}

static gboolean delete_event(GtkWidget *window, GdkEvent *event, gpointer widget) {
	destroy(window, widget);

	return TRUE;
}

//"About" toolbar button handler
void onAboutClicked(GtkWidget *widget, gpointer window) {
	GtkWidget *dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "World Viewer");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.2");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "(c) Team 2");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
			"World Viewer is a program to create a real-time visual representation of the 'Antix' simulation.");
	const gchar
			*authors[2] = {
					"Peter Neufeld, Frank Lau, Egor Philippov,\nYouyou Yang, Jianfeng Hu, Roy Chiang,\nWilson Huynh, Gordon Leung, Kevin Fahy,\nBenjamin Saunders",
					NULL };

	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
}

//"Navigation" and "Info" toolbar button handler
void onWindowToggled(GtkWidget *widget, gpointer window) {
	GdkColor bgColor;
	gdk_color_parse("black", &bgColor);
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &bgColor);

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		//let's update the info window in case we open it
		updateInfoWindow();
		gtk_widget_show_all(GTK_WIDGET(window));
	} else
		gtk_widget_hide_all(GTK_WIDGET(window));

}

//Fullscreen button handler
void onFullscreenToggled(GtkWidget *widget, gpointer window) {

	//we will not be able to see any other windows when in fullscreen mode,
	//so might as well disable the buttons
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Navigation" )), false);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Info" )), false);
		gtk_container_set_border_width(GTK_CONTAINER (window), 0);
		gtk_window_fullscreen(GTK_WINDOW(window));
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Navigation" )), true);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Info" )), true);
		gtk_container_set_border_width(GTK_CONTAINER (window), 10);
		gtk_window_unfullscreen(GTK_WINDOW(window));
	}
}

//called on the drawingArea's expose event
gboolean drawingAreaExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	if (draw[(int) data]) {
		cairo_t *cr = gdk_cairo_create(worldDrawingArea.at((int) data)->widget.window);

		UnpackImage(cr, &renderDraw[(int) data], drawFactor, robotAlpha, &worldInfo,
				fdToRegionId.at(viewedRegion[(int) data].connection->fd));

		cairo_destroy(cr);
		draw[(int) data] = false;
	}

	return FALSE;
}

//zoom in button handler
void onZoomInClicked(GtkWidget *widgetDrawingArea, gpointer data) {
	drawFactor += WVZOOMSPEED;

	if (drawFactor > WVMAXZOOMED) {
		drawFactor -= WVZOOMSPEED;
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomIn" )), false);
#ifdef DEBUG
		debug<<"Failed to zoom in because we have passed the WVMAXZOOMED threshold"<<endl;
#endif
	} else {
		if (!gtk_widget_get_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomOut" ))))
			gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomOut" )), true);

		updateDrawFactor();

#ifdef DEBUG
		debug<<"Zoomed In"<<endl;
#endif
	}
}

//zoom out button hundler
void onZoomOutClicked(GtkWidget *widgetDrawingArea, gpointer data) {
	drawFactor -= WVZOOMSPEED;
	if (drawFactor < WVMINZOOMED) {
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomOut" )), false);
		drawFactor += WVZOOMSPEED;
#ifdef DEBUG
		debug<<"Failed to zoom out because we have passed the WVMINZOOMED threshold"<<endl;
#endif
	} else {
		if (!gtk_widget_get_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomIn" ))))
			gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "ZoomIn" )), true);

		updateDrawFactor();
#ifdef DEBUG
		debug<<"Zoomed Out"<<endl;
#endif
	}
}

//initializations and simple modifications for the things that will be drawn
void initWorldViewer() {
	g_type_init();

	GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton *navigation = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));
	InfoToolButton = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ));
	GtkToggleToolButton *fullscreen = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Fullscreen" ));
	GtkToolButton *zoomIn = GTK_TOOL_BUTTON(gtk_builder_get_object(builder, "ZoomIn"));
	GtkToolButton *zoomOut = GTK_TOOL_BUTTON(gtk_builder_get_object(builder, "ZoomOut"));
	GtkWidget *about = GTK_WIDGET(gtk_builder_get_object( builder, "About" ));
	GtkWidget *infoWindow = GTK_WIDGET(gtk_builder_get_object( builder, "infoWindow" ));
	GtkWidget *navigationWindow = GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));
	GtkWidget *upButton = GTK_WIDGET(gtk_builder_get_object( builder, "up" ));
	GtkWidget *downButton = GTK_WIDGET(gtk_builder_get_object( builder, "down" ));
	GtkWidget *backButton = GTK_WIDGET(gtk_builder_get_object( builder, "back" ));
	GtkWidget *forwardButton = GTK_WIDGET(gtk_builder_get_object( builder, "forward" ));
	GdkColor color;

	gtk_window_set_keep_above(GTK_WINDOW(infoWindow), true);
	gtk_window_set_keep_above(GTK_WINDOW(navigationWindow), true);

	//change the color of the main window's background to black
	gdk_color_parse("black", &color);
	gtk_widget_modify_bg(GTK_WIDGET(mainWindow), GTK_STATE_NORMAL, &color);

	//change the color of the label's text to white
	gdk_color_parse("white", &color);
	for (int frame = 1; frame <= DRAWINGORIGINS; frame++) {
		gtk_widget_modify_fg(
				GTK_WIDGET(gtk_builder_get_object( builder,("frame_frameNum"+helper::toString(frame)).c_str())),
				GTK_STATE_NORMAL, &color);
		gtk_widget_modify_fg(
				GTK_WIDGET(gtk_builder_get_object( builder,("frame_serverId"+helper::toString(frame)).c_str())),
				GTK_STATE_NORMAL, &color);
		gtk_widget_modify_fg(
				GTK_WIDGET(gtk_builder_get_object( builder,("frame_serverAdd"+helper::toString(frame)).c_str())),
				GTK_STATE_NORMAL, &color);
		gtk_widget_modify_fg(
				GTK_WIDGET(gtk_builder_get_object( builder,("frame_serverLoc"+helper::toString(frame)).c_str())),
				GTK_STATE_NORMAL, &color);
	}

	GtkDrawingArea* tmp;
	string tmpStr;
	//set default size, color and keep track of the drawing areas
	for (int i = TOP_LEFT; i <= BOTTOM_RIGHT; i++) {
		tmpStr = "drawingarea" + helper::toString(i + 1);
		tmp = GTK_DRAWING_AREA(gtk_builder_get_object( builder, tmpStr.c_str()));
		gtk_widget_set_size_request(GTK_WIDGET(tmp), IMAGEWIDTH * drawFactor, IMAGEHEIGHT * drawFactor);
		gtk_widget_modify_bg(GTK_WIDGET(tmp), GTK_STATE_NORMAL, &color);
		worldDrawingArea.push_back(tmp);
	}

	g_signal_connect(worldDrawingArea.at(TOP_LEFT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)TOP_LEFT);
	g_signal_connect(worldDrawingArea.at(TOP_RIGHT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)TOP_RIGHT);
	g_signal_connect(worldDrawingArea.at(BOTTOM_LEFT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)BOTTOM_LEFT);
	g_signal_connect(worldDrawingArea.at(BOTTOM_RIGHT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)BOTTOM_RIGHT);

	g_signal_connect(zoomIn, "clicked", G_CALLBACK(onZoomInClicked), (gpointer)NULL);
	g_signal_connect(zoomOut, "clicked", G_CALLBACK(onZoomOutClicked), (gpointer)NULL);
	g_signal_connect(navigation, "toggled", G_CALLBACK(onWindowToggled), (gpointer) navigationWindow);
	g_signal_connect(InfoToolButton, "toggled", G_CALLBACK(onWindowToggled), (gpointer) infoWindow);
	g_signal_connect(fullscreen, "toggled", G_CALLBACK(onFullscreenToggled), (gpointer) mainWindow);
	g_signal_connect(about, "clicked", G_CALLBACK(onAboutClicked), (gpointer) mainWindow);
	g_signal_connect(upButton, "clicked", G_CALLBACK(onUpButtonClicked), (gpointer) mainWindow);
	g_signal_connect(downButton, "clicked", G_CALLBACK(onDownButtonClicked), (gpointer) mainWindow);
	g_signal_connect(backButton, "clicked", G_CALLBACK(onBackButtonClicked), (gpointer) mainWindow);
	g_signal_connect(forwardButton, "clicked", G_CALLBACK(onForwardButtonClicked), (gpointer) mainWindow);

	g_signal_connect(navigationWindow, "destroy", G_CALLBACK(destroy), (gpointer)navigation);
	g_signal_connect(infoWindow, "destroy", G_CALLBACK(destroy), (gpointer)InfoToolButton);
	g_signal_connect(navigationWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)navigation);
	g_signal_connect(infoWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)InfoToolButton);

	gtk_widget_show_all(mainWindow);

	gtk_main();
}

int main(int argc, char* argv[]) {
	gtk_init(&argc, &argv);

	char clockip[40];
	helper::CmdLine cmdline(argc, argv);

	//assume that the worldviewer.builder is in the same directory as the executable that we are running
	string builderPath(argv[0]);
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1) + "worldviewer.glade";
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	gtk_builder_connect_signals(builder, NULL);

	//for the config filename
	string configFileName = cmdline.getArg("-c", "config");

	userAddon = cmdline.getArg("-u", "");
	if (userAddon != "") {
		userAddon = userAddon + "@";

		//sshkeyloc = "exec ssh-agent $BASH >> /dev/null; ssh-add " + sshkeyloc + "; ";
		sshpid = cmdline.getArg("-p", "");
		if (sshpid == "")
			throw runtime_error("Tunnel used, but no auth pid");
		sshsock = cmdline.getArg("-s", "");
		if (sshsock == "")
			throw runtime_error("Tunnel used, but no auth sock");
		sshkeyloc = "SSH_AGENT_PID=" + sshpid + "; SSH_AUTH_SOCK=" + sshsock + "; ";
	}

	//for triggering an ssh tunnel
	tunnelPort = atoi(cmdline.getArg("-t", "-1").c_str());
#ifdef DEBUG
	debug.open(helper::worldViewerDebugLogName.c_str(), ios::out);
#endif
	loadConfigFile(configFileName.c_str(), clockip);

	if (cmdline.getArg("-l").length())
		strncpy(clockip, cmdline.getArg("-l").c_str(), 40);

	//connect to the clock server
	int clockfd = net::do_connect(clockip, WORLD_VIEWER_PORT);

	if (clockfd < 0) {
		cerr << "Critical Error: Failed to connect to the clock server: " << clockip << endl;
		exit(1);
	}
	net::set_blocking(clockfd, false);

	//handle clock messages
	MessageReader clockReader(clockfd);
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, clockMessage, (gpointer) &clockReader);

#ifdef DEBUG
	debug << "Connected to Clock Server " << clockip << endl;
#endif

	initWorldViewer();

#ifdef DEBUG
	debug.close();
#endif
	g_object_unref(G_OBJECT( builder ));

	return 0;
}
