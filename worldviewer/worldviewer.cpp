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

//define the size of the table
const guint tableWorldViewRows = 2, tableWorldViewColumns = 2;

//Variable declarations
/////////////////////////////////////////////////////
vector<regionConnection*> regions;
vector<GtkDrawingArea*> worldDrawingArea;
bool horizontalView = true;
GtkBuilder *builder;
bool runForTheFirstTime = true;
uint32 worldServerRows = 0, worldServerColumns = 0;
map<int, map<int, GtkDrawingArea*> > worldGrid;
RegionRender renderDraw[3];
bool draw[3];
int tunnelPort = -1;
int lastTunnelPort = 12345;
string userAddon;
string sshkeyloc;
string sshpid;
string sshsock;

//this is the region that the navigation will move the grid around
regionConnection *pivotRegion = NULL, *pivotRegionBuddy = NULL;

//used to draw the homes
WorldInfo worldInfo;
map<int, unsigned int> fdToRegionId;

//used only for calculating images per second for each server
int timeCache, lastSecond1 = 0, lastSecond2 = 0, messages1 = 0, messages2 = 0;
int robotSize = 1;
double robotAlpha = 1.0;

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

	if (configuration.find("ROBOTSIZE") != configuration.end())
		robotSize = atoi(configuration["ROBOTSIZE"].c_str());

	if (configuration.find("ROBOTALPHA") != configuration.end())
		robotAlpha = atof(configuration["ROBOTALPHA"].c_str());
}

//display the worldView that we received from a region server in its worldDrawingArea space
void displayWorldView(int regionNum, RegionRender render) {
	int position = BOTTOM_LEFT;

	if (regions.at(regionNum) == pivotRegion)
		position = TOP_LEFT;
	else if (horizontalView && regions.at(regionNum) == pivotRegionBuddy)
		position = TOP_RIGHT;

	renderDraw[position] = render;
	draw[position] = true;
	gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(position)));

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

	gtk_widget_modify_bg(GTK_WIDGET(worldGrid[pivotRegion->info.draw_x()][pivotRegion->info.draw_y()]),
			GTK_STATE_NORMAL, &fgColor);
	gtk_widget_modify_bg(GTK_WIDGET(worldGrid[pivotRegionBuddy->info.draw_x()][pivotRegionBuddy->info.draw_y()]),
			GTK_STATE_NORMAL, &fgColor);
}

//update the info window
void updateInfoWindow() {
	const string frameNumName = "frame_frameNum", frameserverAddName = "frame_serverAdd",
			frameserverLocName = "frame_serverLoc";
	string tmp, position;
	regionConnection *pivotPtr = pivotRegion;

	for (int frame = 1; frame < 3; frame++) {
		if (frame == 2)
			pivotPtr = pivotRegionBuddy;

		GtkLabel
				*frameNum = GTK_LABEL(gtk_builder_get_object( builder, (frameNumName+helper::toString(frame)).c_str() ));
		GtkLabel
				*frameserverAdd = GTK_LABEL(gtk_builder_get_object( builder, (frameserverAddName+helper::toString(frame)).c_str() ));
		GtkLabel
				*frameserverLoc = GTK_LABEL(gtk_builder_get_object( builder, (frameserverLocName+helper::toString(frame)).c_str() ));

		if (frame == 1 || pivotRegionBuddy == pivotRegion)
			position = "top-left";
		else if (horizontalView)
			position = "top-right";
		else
			position = "bottom";

		tmp = "Frame Number: " + helper::toString(frame) + " position in the " + position + " corner";
		gtk_label_set_text(frameNum, tmp.c_str());

		tmp = "Server address: " + helper::toString(pivotPtr->info.address()) + ":" + helper::toString(
				pivotPtr->info.renderport()) + " connected on fd = " + helper::toString(pivotPtr->fd);
		gtk_label_set_text(frameserverAdd, tmp.c_str());

		tmp = "Server located at: (" + helper::toString(pivotPtr->info.draw_x()) + ", " + helper::toString(
				pivotPtr->info.draw_y()) + " )";
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
void benchmarkMessages(regionConnection *region) {
	timeCache = time(NULL);

	if (region == pivotRegion)
		messages1++;
	else
		messages2++;

	//check if its time to output
	if (timeCache > lastSecond1 || timeCache > lastSecond2) {
		string tmp;

		if (region == pivotRegion) {
			GtkLabel *frameserverId = GTK_LABEL(gtk_builder_get_object( builder, "frame_serverId1" ));
			tmp = "Server ID: " + helper::toString(pivotRegion->info.id()) + " sending at " + helper::toString(
					messages1) + " images per second";
			gtk_label_set_text(frameserverId, tmp.c_str());

			messages1 = 0;
			lastSecond1 = timeCache;
		}
		if (region == pivotRegionBuddy) {
			GtkLabel *frameserverId = GTK_LABEL(gtk_builder_get_object( builder, "frame_serverId2" ));
			tmp = "Server ID: " + helper::toString(pivotRegionBuddy->info.id()) + " sending at " + helper::toString(
					messages2) + " images per second";
			gtk_label_set_text(frameserverId, tmp.c_str());

			messages2 = 0;
			lastSecond2 = timeCache;
		}
	}
}

//handler for region received messages
gboolean regionMessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();
	MessageType type;
	int len;
	const void *buffer;
	int regionNum = 0;

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

		//if you don't have a buddy then you're your own buddy ='(
		if (pivotRegionBuddy == NULL)
			pivotRegionBuddy = pivotRegion;

		initializeToolbarButtons();
	}

	switch (type) {
	case MSG_REGIONVIEW: {
		RegionRender render;

		render.ParseFromArray(buffer, len);

#ifdef DEBUG
		debug << "Received render update from server fd=" << regions.at(regionNum)->fd << " and the timestep is # "
				<< render.timestep() << endl;
#endif
		benchmarkMessages(regions.at(regionNum));

		displayWorldView(regionNum, render);

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

		int regionFd;
		//setup an ssh tunnel first
		if(tunnelPort != -1)
		{
		  char * stringAddr = inet_ntoa(addr);
		  stringstream tunnelcmd;

		  tunnelcmd << sshkeyloc << "ssh -f -N -p" << tunnelPort << " -L " << lastTunnelPort << ":127.0.0.1:" << regioninfo.renderport() << " " << userAddon << stringAddr;

      //setup the tunnel
      if(system(tunnelcmd.str().c_str()) != 0)
        throw runtime_error("Unable to establish ssh connection");

		  addr.s_addr = inet_addr("127.0.0.1");
		  regionFd = net::do_connect(addr, lastTunnelPort);
		  lastTunnelPort++;
		  //cout << tunnelcmd.str() << endl;
		}else
		  regionFd = net::do_connect(addr, regioninfo.renderport());

		net::set_blocking(regionFd, false);
		fdToRegionId[regionFd]=regioninfo.id();

		//when the world viewer starts it only shows a horizontal rectangle of world views from region servers (0,0) and (0,1)
		if (regioninfo.draw_x() == 0 && regioninfo.draw_y() == 0) {
			gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_LEFT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegion = regions.at(regions.size() - 1);
			sendWorldViews(pivotRegion->fd, true);
		} else if (regioninfo.draw_x() == 1 && regioninfo.draw_y() == 0) {
			gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegionBuddy = regions.at(regions.size() - 1);
			sendWorldViews(pivotRegionBuddy->fd, true);
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

//see if the buddy of the pivot is the pivot itself
void compareBuddy() {
	if (pivotRegion == pivotRegionBuddy) {
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), 0, 0);
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)), 0, 0);

		gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)));
		gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)));
#ifdef DEBUG
		debug << "pivtorBuddy is the same as the pivotRegion. Removing buddy!" << endl;
#endif
	}
}

//find and set the new pivotRegion and its buddy from the new coordinates
void setNewRegionPivotAndBuddy(uint32 newPivotRegion[], uint32 newPivotRegionBuddy[]) {

#ifdef DEBUG
	debug << "Changing pivotRegion to (" << newPivotRegion[0] << "," << newPivotRegion[1]
			<< ") and pivotRegionBuddy to (" << newPivotRegionBuddy[0] << "," << newPivotRegionBuddy[1] << ")" << endl;
#endif

	for (int i = 0; i < (int) regions.size(); i++) {
		if (regions.at(i)->info.draw_x() == newPivotRegion[0] && regions.at(i)->info.draw_y() == newPivotRegion[1]) {
			pivotRegion = regions.at(i);
		}
		if (regions.at(i)->info.draw_x() == newPivotRegionBuddy[0] && regions.at(i)->info.draw_y()
				== newPivotRegionBuddy[1]) {
			pivotRegionBuddy = regions.at(i);
		}
	}

	compareBuddy();
	updateWorldGrid("light green");
	//only update the info window if the its button is toggled
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ))))
		updateInfoWindow();
}

//up button handler
void onDownButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked down" << endl;
#endif
	uint32 tmp1, tmp2;

	//update the world grid to have no currently viewed regions
	updateWorldGrid("white");

	//find the new positions of the region and its buddy
	if (pivotRegion->info.draw_y() + 1 >= worldServerRows)
		tmp1 = 0;
	else
		tmp1 = pivotRegion->info.draw_y() + 1;

	if (pivotRegionBuddy->info.draw_y() + 1 >= worldServerRows)
		tmp2 = 0;
	else
		tmp2 = pivotRegionBuddy->info.draw_y() + 1;

	uint32 newPivotRegion[2] = { pivotRegion->info.draw_x(), tmp1 };
	uint32 newPivotRegionBuddy[2] = { pivotRegionBuddy->info.draw_x(), tmp2 };

	//disable the sending of world views from regions that we are moving away from
	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, false);
		sendWorldViews(pivotRegionBuddy->fd, false);
	} else {
		sendWorldViews(pivotRegion->fd, false);
	}
	//set the new pivotRegion and pivotRegionBuddy
	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);

	//enable the sending of world views from regions that we are moving to
	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, true);
		sendWorldViews(pivotRegionBuddy->fd, true);
	} else {
		sendWorldViews(pivotRegionBuddy->fd, true);
	}

}

//down button handler
void onUpButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked up" << endl;
#endif
	uint32 tmp1, tmp2;

	updateWorldGrid("white");

	if ((int) pivotRegion->info.draw_y() - 1 < 0)
		tmp1 = worldServerRows - 1;
	else
		tmp1 = pivotRegion->info.draw_y() - 1;

	if ((int) pivotRegionBuddy->info.draw_y() - 1 < 0)
		tmp2 = worldServerRows - 1;
	else
		tmp2 = pivotRegionBuddy->info.draw_y() - 1;

	uint32 newPivotRegion[2] = { pivotRegion->info.draw_x(), tmp1 };
	uint32 newPivotRegionBuddy[2] = { pivotRegionBuddy->info.draw_x(), tmp2 };

	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, false);
		sendWorldViews(pivotRegionBuddy->fd, false);
	} else {
		sendWorldViews(pivotRegionBuddy->fd, false);
	}

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);

	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, true);
		sendWorldViews(pivotRegionBuddy->fd, true);
	} else {
		sendWorldViews(pivotRegion->fd, true);
	}
}

//back button handler
void onBackButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked back" << endl;
#endif
	uint32 tmp1, tmp2;

	updateWorldGrid("white");

	if (((int) pivotRegion->info.draw_x()) - 1 < 0)
		tmp1 = worldServerColumns - 1;
	else
		tmp1 = pivotRegion->info.draw_x() - 1;

	if (((int) pivotRegionBuddy->info.draw_x()) - 1 < 0)
		tmp2 = worldServerColumns - 1;
	else
		tmp2 = pivotRegionBuddy->info.draw_x() - 1;

	uint32 newPivotRegion[2] = { tmp1, pivotRegion->info.draw_y() };
	uint32 newPivotRegionBuddy[2] = { tmp2, pivotRegionBuddy->info.draw_y() };

	if (horizontalView) {
		sendWorldViews(pivotRegionBuddy->fd, false);
	} else {
		sendWorldViews(pivotRegion->fd, false);
		sendWorldViews(pivotRegionBuddy->fd, false);
	}

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);

	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, true);
	} else {
		sendWorldViews(pivotRegion->fd, true);
		sendWorldViews(pivotRegionBuddy->fd, true);
	}
}

//forward button handler
void onForwardButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked forward" << endl;
#endif
	uint32 tmp1, tmp2;

	updateWorldGrid("white");

	if (pivotRegion->info.draw_x() + 1 >= worldServerColumns)
		tmp1 = 0;
	else
		tmp1 = pivotRegion->info.draw_x() + 1;

	if (pivotRegionBuddy->info.draw_x() + 1 >= worldServerColumns)
		tmp2 = 0;
	else
		tmp2 = pivotRegionBuddy->info.draw_x() + 1;

	uint32 newPivotRegion[2] = { tmp1, pivotRegion->info.draw_y() };
	uint32 newPivotRegionBuddy[2] = { tmp2, pivotRegionBuddy->info.draw_y() };

	if (horizontalView) {
		sendWorldViews(pivotRegion->fd, false);
	} else {
		sendWorldViews(pivotRegion->fd, false);
		sendWorldViews(pivotRegionBuddy->fd, false);
	}

	setNewRegionPivotAndBuddy(newPivotRegion, newPivotRegionBuddy);

	if (horizontalView) {
		sendWorldViews(pivotRegionBuddy->fd, true);
	} else {
		sendWorldViews(pivotRegion->fd, true);
		sendWorldViews(pivotRegionBuddy->fd, true);
	}
}

//rotate button handler
void onRotateButtonClicked(GtkWidget *widget, gpointer window) {
#ifdef DEBUG
	debug << "Clicked rotate" << endl;
#endif
	uint32 newPivotRegionBuddy[2], tmp1 = pivotRegion->info.draw_x(), tmp2 = pivotRegion->info.draw_y();
	horizontalView = !horizontalView;

	updateWorldGrid("white");

	//which way do we want to rotate?
	if (horizontalView) {
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)), 0, 0);

		if (pivotRegion->info.draw_x() + 1 >= worldServerColumns)
			tmp1 = 0;
		else
			tmp1 += 1;
	} else {
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)), IMAGEWIDTH, IMAGEHEIGHT);
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), 0, 0);

		if (pivotRegion->info.draw_y() + 1 >= worldServerRows)
			tmp2 = 0;
		else
			tmp2 += 1;
	}

	newPivotRegionBuddy[0] = tmp1;
	newPivotRegionBuddy[1] = tmp2;

#ifdef DEBUG
	debug << "Changing pivotRegionBuddy to (" << newPivotRegionBuddy[0] << "," << newPivotRegionBuddy[1] << ")" << endl;
#endif

	if (pivotRegionBuddy != pivotRegion)
		sendWorldViews(pivotRegionBuddy->fd, false);

	for (int i = 0; i < (int) regions.size(); i++) {
		if (regions.at(i)->info.draw_x() == newPivotRegionBuddy[0] && regions.at(i)->info.draw_y()
				== newPivotRegionBuddy[1])
			pivotRegionBuddy = regions.at(i);
	}

	if (pivotRegionBuddy != pivotRegion)
		sendWorldViews(pivotRegionBuddy->fd, true);

	compareBuddy();
	updateWorldGrid("light green");

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ))))
		updateInfoWindow();
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
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.1");
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
		unsigned int regionId;

		if( (int)data == TOP_LEFT )
			regionId=fdToRegionId.at(pivotRegion->fd);
		else
			regionId=fdToRegionId.at(pivotRegionBuddy->fd);

		UnpackImage(cr, &renderDraw[(int) data], robotSize, robotAlpha, &worldInfo, regionId);

		cairo_destroy(cr);
		draw[(int) data] = false;
	}

	return FALSE;
}

//initializations and simple modifications for the things that will be drawn
void initWorldViewer() {
	g_type_init();

	GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton *navigation = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));
	GtkToggleToolButton *info = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ));
	GtkToggleToolButton *fullscreen = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Fullscreen" ));
	GtkWidget *about = GTK_WIDGET(gtk_builder_get_object( builder, "About" ));
	GtkWidget *infoWindow = GTK_WIDGET(gtk_builder_get_object( builder, "infoWindow" ));
	GtkWidget *navigationWindow = GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));
	GtkWidget *table = GTK_WIDGET(gtk_builder_get_object( builder, "tableWorldView" ));
	GtkWidget *upButton = GTK_WIDGET(gtk_builder_get_object( builder, "up" ));
	GtkWidget *downButton = GTK_WIDGET(gtk_builder_get_object( builder, "down" ));
	GtkWidget *backButton = GTK_WIDGET(gtk_builder_get_object( builder, "back" ));
	GtkWidget *forwardButton = GTK_WIDGET(gtk_builder_get_object( builder, "forward" ));
	GtkWidget *rotateButton = GTK_WIDGET(gtk_builder_get_object( builder, "rotate" ));
	GdkColor color;

	draw[0] = false;
	draw[1] = false;
	draw[2] = false;
	gtk_window_set_keep_above(GTK_WINDOW(infoWindow), true);
	gtk_window_set_keep_above(GTK_WINDOW(navigationWindow), true);

	//change the color of the main window's background to black
	gdk_color_parse("black", &color);
	gtk_widget_modify_bg(GTK_WIDGET(mainWindow), GTK_STATE_NORMAL, &color);

	//change the color of the label's text to white
	gdk_color_parse("white", &color);
	for (int frame = 1; frame < 3; frame++) {
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

	//create a grid to display the received world views
	for (guint i = 0; i < tableWorldViewRows; i++) {
		for (guint j = 0; j < tableWorldViewColumns; j++) {
			GtkDrawingArea* area = GTK_DRAWING_AREA(gtk_drawing_area_new());
			gtk_widget_modify_bg(GTK_WIDGET(area), GTK_STATE_NORMAL, &color);
			gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(area), j, j + 1, i, i + 1, GTK_FILL, GTK_FILL, 0, 0);
			worldDrawingArea.push_back(GTK_DRAWING_AREA(area));
		}
	}

	g_signal_connect(worldDrawingArea.at(TOP_LEFT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)TOP_LEFT);
	g_signal_connect(worldDrawingArea.at(TOP_RIGHT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)TOP_RIGHT);
	g_signal_connect(worldDrawingArea.at(BOTTOM_LEFT), "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)BOTTOM_LEFT);

	g_signal_connect(navigation, "toggled", G_CALLBACK(onWindowToggled), (gpointer) navigationWindow);
	g_signal_connect(info, "toggled", G_CALLBACK(onWindowToggled), (gpointer) infoWindow);
	g_signal_connect(fullscreen, "toggled", G_CALLBACK(onFullscreenToggled), (gpointer) mainWindow);
	g_signal_connect(about, "clicked", G_CALLBACK(onAboutClicked), (gpointer) mainWindow);
	g_signal_connect(upButton, "clicked", G_CALLBACK(onUpButtonClicked), (gpointer) mainWindow);
	g_signal_connect(downButton, "clicked", G_CALLBACK(onDownButtonClicked), (gpointer) mainWindow);
	g_signal_connect(backButton, "clicked", G_CALLBACK(onBackButtonClicked), (gpointer) mainWindow);
	g_signal_connect(forwardButton, "clicked", G_CALLBACK(onForwardButtonClicked), (gpointer) mainWindow);
	g_signal_connect(rotateButton, "clicked", G_CALLBACK(onRotateButtonClicked), (gpointer) mainWindow);

	g_signal_connect(navigationWindow, "destroy", G_CALLBACK(destroy), (gpointer)navigation);
	g_signal_connect(infoWindow, "destroy", G_CALLBACK(destroy), (gpointer)info);
	g_signal_connect(navigationWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)navigation);
	g_signal_connect(infoWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)info);

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
	if(userAddon != "")
	{
	  userAddon = userAddon + "@";

	  //sshkeyloc = "exec ssh-agent $BASH >> /dev/null; ssh-add " + sshkeyloc + "; ";
	  sshpid = cmdline.getArg("-p", "");
	  if(sshpid == "")
	    throw runtime_error("Tunnel used, but no auth pid");
	  sshsock = cmdline.getArg("-s", "");
	  if(sshsock == "")
	    throw runtime_error("Tunnel used, but no auth sock");
	  sshkeyloc = "SSH_AGENT_PID=" + sshpid + "; SSH_AUTH_SOCK=" + sshsock + "; ";
	}

	//for triggering an ssh tunnel
	tunnelPort = atoi(cmdline.getArg("-t", "-1").c_str());
#ifdef DEBUG
	debug.open(helper::worldViewerDebugLogName.c_str(), ios::out);
#endif
	loadConfigFile(configFileName.c_str(), clockip);

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
