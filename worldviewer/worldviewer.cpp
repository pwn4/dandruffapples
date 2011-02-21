/*////////////////////////////////////////////////////////////////////////////////////////////////
 WorldViewer program
 This program communications with the clock server and region servers
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
vector<GtkDrawingArea*> worldDrawingArea;
bool horizontalView = true;
GtkBuilder *builder;
bool runForTheFirstTime = true;
uint32 worldServerRows = 0, worldServerColumns = 0;
map<int, map<int, GtkDrawingArea*> > worldGrid;
//this is the region that the navigation will move the grid around
regionConnection *pivotRegion = NULL, *pivotRegionBuddy = NULL;

//used only for calculating images per second for each server
int timeCache, lastSecond1 = 0, lastSecond2 = 0, messages1 = 0, messages2 = 0;

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

//display the worldView that we received from a region server in its worldDrawingArea space
void displayWorldView(int regionNum, RegionRender render) {
	int position = BOTTOM_LEFT;

	if (regions.at(regionNum) == pivotRegion)
		position = TOP_LEFT;
	else if (horizontalView && regions.at(regionNum) == pivotRegionBuddy)
		position = TOP_RIGHT;

	cairo_t *cr = gdk_cairo_create(worldDrawingArea.at(position)->widget.window);
	cairo_surface_t *image = cairo_image_surface_create_for_data((unsigned char*) render.image().c_str(), IMAGEFORMAT,
			IMAGEWIDTH, IMAGEHEIGHT, cairo_format_stride_for_width(IMAGEFORMAT, IMAGEWIDTH));

	cairo_set_source_surface(cr, image, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
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
	const string frameNumName = "frame_frameNum", frameserverAddName = "frame_serverAdd", frameserverLocName =
			"frame_serverLoc";
	string tmp, position;
	regionConnection *pivotPtr = pivotRegion;

	for (int frame = 1; frame < 3; frame++) {
		if (frame == 2)
			pivotPtr = pivotRegionBuddy;

		GtkLabel *frameNum =
				GTK_LABEL(gtk_builder_get_object( builder, (frameNumName+helper::toString(frame)).c_str() ));
		GtkLabel *frameserverAdd =
				GTK_LABEL(gtk_builder_get_object( builder, (frameserverAddName+helper::toString(frame)).c_str() ));
		GtkLabel *frameserverLoc =
				GTK_LABEL(gtk_builder_get_object( builder, (frameserverLocName+helper::toString(frame)).c_str() ));

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
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Navigation" )), true);
	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object( builder, "Info" )), true);

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
void benchmarkMessages(int fd) {
	timeCache = time(NULL);

	if (fd == pivotRegion->fd)
		messages1++;
	else
		messages2++;

	//check if its time to output
	if (timeCache > lastSecond1 || timeCache > lastSecond2) {
		string tmp;

		if (fd == pivotRegion->fd) {
			GtkLabel *frameserverId = GTK_LABEL(gtk_builder_get_object( builder, "frame_serverId1" ));
			tmp = "Server ID: " + helper::toString(pivotRegion->info.id()) + " sending at " + helper::toString(
					messages1) + " images per second";
			gtk_label_set_text(frameserverId, tmp.c_str());

			messages1 = 0;
			lastSecond1 = timeCache;
		}
		if (fd == pivotRegionBuddy->fd) {
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
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();
	MessageType type;
	int len;
	const void *buffer;
	int regionNum = 0;

	//get the region number that we are receiving a world view from
	for (vector<regionConnection*>::iterator it = regions.begin(); it != regions.end() && (*it)->fd
			!= g_io_channel_unix_get_fd(ioch); it++, regionNum++) {
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

	//if the server is not viewed then tell it to stop sending to us
	if (regions.at(regionNum)->fd != pivotRegion->fd && regions.at(regionNum)->fd != pivotRegionBuddy->fd) {
		sendWorldViews(regions.at(regionNum)->fd, false);
	} else {
		switch (type) {
		case MSG_REGIONVIEW: {
			RegionRender render;
			render.ParseFromArray(buffer, len);

#ifdef DEBUG
			debug << "Received render update from server fd=" << regions.at(regionNum)->fd << " and the timestep is # "
					<< render.timestep() << endl;
#endif
			benchmarkMessages(regions.at(regionNum)->fd);

			displayWorldView(regionNum, render);

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

		//when the world viewer starts it only shows a horizontal rectangle of world views from region servers (0,0) and (0,1)
		if (regioninfo.draw_x() == 0 && regioninfo.draw_y() == 0) {
			gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_LEFT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegion = regions.at(regions.size() - 1);
		} else if (regioninfo.draw_x() == 1 && regioninfo.draw_y() == 0) {
			gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), IMAGEWIDTH, IMAGEHEIGHT);
			regions.push_back(new regionConnection(regionFd, regioninfo));
			pivotRegionBuddy = regions.at(regions.size() - 1);
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
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN, io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server fd=" << regionFd << " located at ( " << regioninfo.draw_x() << ", "
				<< regioninfo.draw_y() << " )" << endl;
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
void on_downButton_clicked(GtkWidget *widget, gpointer window) {
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
void on_upButton_clicked(GtkWidget *widget, gpointer window) {
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
void on_backButton_clicked(GtkWidget *widget, gpointer window) {
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
void on_forwardButton_clicked(GtkWidget *widget, gpointer window) {
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
void on_rotateButton_clicked(GtkWidget *widget, gpointer window) {
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

		gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)));

		if (pivotRegion->info.draw_x() + 1 >= worldServerColumns)
			tmp1 = 0;
		else
			tmp1 += 1;
	} else {
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(BOTTOM_LEFT)), IMAGEWIDTH, IMAGEHEIGHT);
		gtk_widget_set_size_request(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)), 0, 0);

		gtk_widget_queue_draw(GTK_WIDGET(worldDrawingArea.at(TOP_RIGHT)));


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
	gtk_widget_hide_all((GtkWidget*) window);
	gtk_toggle_tool_button_set_active((GtkToggleToolButton*) widget, FALSE);
}

static gboolean delete_event(GtkWidget *window, GdkEvent *event, gpointer widget) {
	destroy(window, widget);

	return TRUE;
}

//"About" toolbar button handler
void on_About_clicked(GtkWidget *widget, gpointer window) {
	GtkWidget *dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "World Viewer");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.1");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "(c) Team 2");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
			"World Viewer is a program to create a real-time visual representation of the 'Antix' simulation.");
	const gchar
			*authors[2] =
					{
							"Peter Neufeld, Frank Lau, Egor Philippov,\nYouyou Yang, Jianfeng Hu, Roy Chiang,\nWilson Huynh, Gordon Leugn, Kevin Fahy,\nBenjamin Saunders",
							NULL };

	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
}

//"Navigation" and "Info" toolbar button handler
void on_Window_toggled(GtkWidget *widget, gpointer window) {
	GdkColor bgColor;
	gdk_color_parse("black", &bgColor);
	gtk_widget_modify_bg(GTK_WIDGET((GtkWidget*)window), GTK_STATE_NORMAL, &bgColor);

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget))) {
		gtk_widget_show_all((GtkWidget*) window);
	} else {
		gtk_widget_hide_all((GtkWidget*) window);
	}
}

//initializations and simple modifications for the things that will be drawn
void initializeDrawers() {
	g_type_init();
	guint rows = 2, columns = 2;
	GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton *navigation = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));
	GtkToggleToolButton *info = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Info" ));
	GtkWidget *about = GTK_WIDGET(gtk_builder_get_object( builder, "About" ));
	GtkWidget *infoWindow = GTK_WIDGET(gtk_builder_get_object( builder, "infoWindow" ));
	GtkWidget *navigationWindow = GTK_WIDGET(gtk_builder_get_object( builder, "navigationWindow" ));
	GtkWidget *table = GTK_WIDGET(gtk_builder_get_object( builder, "table" ));
	GtkWidget *upButton = GTK_WIDGET(gtk_builder_get_object( builder, "up" ));
	GtkWidget *downButton = GTK_WIDGET(gtk_builder_get_object( builder, "down" ));
	GtkWidget *backButton = GTK_WIDGET(gtk_builder_get_object( builder, "back" ));
	GtkWidget *forwardButton = GTK_WIDGET(gtk_builder_get_object( builder, "forward" ));
	GtkWidget *rotateButton = GTK_WIDGET(gtk_builder_get_object( builder, "rotate" ));

	//change the color of the main window's background to black
	GdkColor bgColor;
	gdk_color_parse("black", &bgColor);
	gtk_widget_modify_bg(GTK_WIDGET(mainWindow), GTK_STATE_NORMAL, &bgColor);

	//create a grid to display the received world views
	for (guint i = 0; i < rows; i++) {
		for (guint j = 0; j < columns; j++) {
			GtkDrawingArea* area = GTK_DRAWING_AREA(gtk_drawing_area_new());
			gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(area), j, j + 1, i, i + 1);
			worldDrawingArea.push_back(GTK_DRAWING_AREA(area));
		}
	}

	g_signal_connect(navigation, "toggled", G_CALLBACK(on_Window_toggled), (gpointer) navigationWindow);
	g_signal_connect(info, "toggled", G_CALLBACK(on_Window_toggled), (gpointer) infoWindow);
	g_signal_connect(about, "clicked", G_CALLBACK(on_About_clicked), (gpointer) mainWindow);
	g_signal_connect(upButton, "clicked", G_CALLBACK(on_upButton_clicked), (gpointer) mainWindow);
	g_signal_connect(downButton, "clicked", G_CALLBACK(on_downButton_clicked), (gpointer) mainWindow);
	g_signal_connect(backButton, "clicked", G_CALLBACK(on_backButton_clicked), (gpointer) mainWindow);
	g_signal_connect(forwardButton, "clicked", G_CALLBACK(on_forwardButton_clicked), (gpointer) mainWindow);
	g_signal_connect(rotateButton, "clicked", G_CALLBACK(on_rotateButton_clicked), (gpointer) mainWindow);

	g_signal_connect(navigationWindow, "destroy", G_CALLBACK(destroy), (gpointer)navigation);
	g_signal_connect(infoWindow, "destroy", G_CALLBACK(destroy), (gpointer)info);
	g_signal_connect(navigationWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)navigation);
	g_signal_connect(infoWindow, "delete-event", G_CALLBACK(delete_event), (gpointer)info);

	gtk_widget_show_all(mainWindow);

	gtk_main();
}

int main(int argc, char* argv[]) {
	map<int, bool> test;

	gtk_init(&argc, &argv);

	char clockip[40];
	helper::Config config(argc, argv);

	//assume that the worldviewer.builder is in the same directory as the executable that we are running
	string builderPath(argv[0]);
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1) + "worldviewer.builder";
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	gtk_builder_connect_signals(builder, NULL);

	const char *configFileName = (config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
#ifdef DEBUG
	debug.open(helper::worldViewerDebugLogName.c_str(), ios::out);
#endif
	loadConfigFile(configFileName, clockip);

	//connect to the clock server
	int clockfd = net::do_connect(clockip, WORLD_VIEWER_PORT);

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