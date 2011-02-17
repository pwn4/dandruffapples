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
#include <stdlib.h>

#include <google/protobuf/message_lite.h>

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

#include "../common/helper.h"
#include <Magick++.h>
#include <gtk/gtk.h>
#include <cairo.h>

using namespace std;
using namespace Magick;

struct regionConnection: net::connection {
	RegionInfo info;
	//temporary int
	int num;

	regionConnection(int fd, RegionInfo info_) :
		net::connection(fd), info(info_) {
	}

};

//variable declarations
MessageType type;
size_t len;
const void *buffer;
ofstream debug;
vector<regionConnection*> regions;
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

cairo_status_t reader(void *blob, unsigned char *data, unsigned int length)
 {
	const void* blobData=((Blob*)blob)->data();
	data= (unsigned char*)blobData;

    return CAIRO_STATUS_SUCCESS;
 }


//display the png that we received from a region server in its pngDrawingArea space
void displayPng(int serverNum, RegionRender render) {
	Blob blob((void*) render.image().c_str(), render.image().length());
	Image blobImage;
	blobImage.read(blob);
	Pixels blobDots(blobImage);
	
  PixelPacket *pixel_cache = blobImage.getPixels(0,0,blobImage.columns(),blobImage.rows());

  PixelPacket *pixel;
  //iterate through the pixels
  for(ssize_t row = 0; row < blobImage.rows(); row++)
    for(ssize_t column = 0; column < blobImage.columns(); column++)
    {
      pixel = pixel_cache+row*blobImage.columns()+column;
      
      //NOTE: color values range from 0 to 65535 (not 255). all values 65535=white.
      //if(pixel->red != 65535 || pixel->green != 65535 || pixel->blue != 65535)
      //  printf("%d %d %d\n", pixel->red, pixel->green, pixel->blue);
    }
	
	//cairo code to be integrated?
  /*
	cairo_t *cr = gdk_cairo_create(pngDrawingArea.at(serverNum)->widget.window);

	cairo_surface_t *image = cairo_image_surface_create_from_png_stream(reader, &blob);

	cairo_set_source_surface(cr, image, 10, 10);
	cairo_paint(cr);

	cairo_destroy(cr);*/
}

//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data) {
	g_type_init();

	int serverNum=0;
	//get the region number that we are receiving a PNG from
	for (vector<regionConnection*>::iterator it = regions.begin(); it
			!= regions.end() && (*it)->fd != g_io_channel_unix_get_fd(ioch); it++, serverNum++){}

	for (bool complete = false; !complete;)
		complete = regions.at(serverNum)->reader.doRead(&type, &len, &buffer);

	switch (type) {
	case MSG_REGIONRENDER: {
		RegionRender render;
		render.ParseFromArray(buffer, len);
#ifdef DEBUG
		debug << "Received MSG_REGIONRENDER update and the timestep is # "
				<< render.timestep() << endl;
#endif
		displayPng(serverNum, render);

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

		//create a new handler to wait for when the server sends a new png to us
		g_io_add_watch(g_io_channel_unix_new(regionFd), G_IO_IN,
				io_regionmessage, NULL);
#ifdef DEBUG
		debug << "Connected to region server: " << addr.s_addr << endl;
#endif

		break;
	}

	default:
	{
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

	if (gtk_toggle_tool_button_get_active(
			GTK_TOGGLE_TOOL_BUTTON(propertiesWindow))) {
		gtk_widget_show_all(propertiesWindow);
	} else {
		gtk_widget_hide_all(propertiesWindow);
	}
}

void drawer(int argc, char* argv[], int clockfd) {
	gtk_init(&argc, &argv);
	g_type_init();

	//assume that the pngviewer.builder is in the same directory as the executable that we are running
	string builderPath(argv[0]);
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1)
			+ "pngviewer.builder";
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	gtk_builder_connect_signals(builder, NULL);

	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object( builder, "window" ));
	GtkToggleToolButton	*navigation =
					GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object( builder, "Navigation" ));

	//todo: this is only hardcoded for 2. Must work for 'n' in the future.
	pngDrawingArea.push_back(GTK_DRAWING_AREA(gtk_builder_get_object( builder, "pngDraw1" )));
	pngDrawingArea.push_back(GTK_DRAWING_AREA(gtk_builder_get_object( builder, "pngDraw2" )));

	MessageReader clockReader(clockfd);

	//adder handler for the clock
	g_io_add_watch(g_io_channel_unix_new(clockfd), G_IO_IN, io_clockmessage,
			(gpointer) &clockReader);
	g_signal_connect(navigation, "toggled", G_CALLBACK(on_Navigation_toggled), (gpointer) window);

	gtk_widget_show_all(window);

	gtk_main();
}

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

	drawer(argc, argv, clockfd);

	debug.close();
	g_object_unref(G_OBJECT( builder ));

	return 0;
}
