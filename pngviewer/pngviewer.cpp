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

//variable declarations
const char *configFileName;
int windowWidth = 900, windowHeight = 900;
char clockip [40] = "127.0.0.1";

int clockfd, listenfd, clientfd;
TimestepUpdate timestep;
MessageReader* reader;
MessageType type;
	GIOChannel *ioch; //event handler
size_t len;
const void *buffer;

struct connection{
RegionInfo info;

int fd;
MessageReader reader;

connection(int fd_, RegionInfo info_) : fd(fd_), reader(fd_), info(info_) {}

};

vector<connection*> region;

void loadConfigFile()
{
	//load the config file
  
  conf configuration = parseconf(configFileName);
  
  //clock ip address
  if(configuration.find("CLOCKIP") == configuration.end()) {
    cerr << "Config file is missing an entry!" << endl;
    exit(1);
  }
  strcpy(clockip, configuration["CLOCKIP"].c_str());

}


//window destruction methods
static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{ return FALSE;  }
static void destroy( GtkWidget *widget,
                     gpointer   data )
{  gtk_main_quit ();  }

static gboolean
on_expose_event(GtkWidget *widget,
    GdkEventExpose *event,
    gpointer data)
{
  //Test drawing function
  cairo_surface_t *image;

  image = cairo_image_surface_create_from_png("rose.png");

  cairo_t *cr;

  cr = gdk_cairo_create (widget->window);

  cairo_set_source_surface(cr, image, 10, 10);
  cairo_paint(cr);

  cairo_destroy(cr);

  return FALSE;
}


//handler for region received messages
gboolean io_regionmessage(GIOChannel *ioch, GIOCondition cond, gpointer data)
{cout << g_io_channel_unix_get_fd(ioch) << endl;
  connection * conn;
  //get the reader for the handle
  for(vector<connection*>::iterator i = region.begin();
                i != region.end(); i++)
    if((*i)->fd == g_io_channel_unix_get_fd(ioch))
    {
      conn = (*i);
      break;
    }
  
  if(!(conn)->reader.doRead(&type, &len, &buffer))
    return TRUE;
  switch (type) {
	  case MSG_REGIONRENDER:
	  {
      cout << "Received MSG_REGIONRENDER update!" << endl;
	    break;
	  }
	  case MSG_TIMESTEPUPDATE:
	  {
	    //cout << "WWWTTF" << endl;
	    break;
    }
	  default:
      cerr << "Unexpected readable socket from region! Type:" << type  << "|" << MSG_REGIONRENDER<< endl;
  }
	
	
	
  return TRUE;
}


//handler for clock received messages
gboolean io_clockmessage(GIOChannel *ioch, GIOCondition cond, gpointer data)
{
  if(!reader->doRead(&type, &len, &buffer))
    return TRUE;
  switch (type) {
	  case MSG_REGIONINFO:
	  {
  		//we got regionserver information
  		RegionInfo regioninfo;
			regioninfo.ParseFromArray(buffer, len);
			cout << "Received MSG_REGIONINFO update! " << regioninfo.address() << " " << regioninfo.renderport() << endl;
			//connect to the server
			struct in_addr addr;
			addr.s_addr = regioninfo.address();
			int fd = net::do_connect(addr, regioninfo.renderport());
			net::set_blocking(fd, false);
			//store the region server mapping
			connection *newregion = new connection(fd, regioninfo);
			region.push_back(newregion);
			GIOChannel *nioch;
			nioch = g_io_channel_unix_new(fd);
			g_io_add_watch(nioch, G_IO_IN, io_regionmessage, NULL); //start watching it
			cout << "Connected to region server!" << endl;
			//update knowledge of the world
      
	    
	    break;
	  }
	
	  default:
      cerr << "Unexpected readable socket from clock! Type:" << type << endl;
  }
	
	
	
  return TRUE;
}


//main method
int main(int argc, char* argv[])
{
  //parse command line arguments and load the config file
  helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
  cout<<"Using config file: "<<configFileName<<endl;
  loadConfigFile();
  
  //connect to the clock server
  clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
  net::set_blocking(clockfd, false);
  reader = new MessageReader(clockfd);
  cout << "Connected to Clock Server " << clockfd << endl;
  
  //create the window object and init
  GtkWidget *window;
  gtk_init (&argc, &argv);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);  
  
  //link window events
  g_signal_connect(window, "expose-event",
      G_CALLBACK(on_expose_event), NULL);
  g_signal_connect (window, "delete-event",
	      G_CALLBACK (delete_event), NULL);
  g_signal_connect (window, "destroy",
	      G_CALLBACK (destroy), NULL);
	      
	//link socket events
	ioch = g_io_channel_unix_new(clockfd);
	g_io_add_watch(ioch, G_IO_IN, io_clockmessage, NULL);

  
  //set the border width of the window.
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
 
  //set some window params
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "PNGViewer");
  gtk_window_set_default_size(GTK_WINDOW(window), windowWidth, windowHeight); 
  gtk_widget_set_app_paintable(window, TRUE);

  gtk_widget_show_all(window);

  gtk_main();


  
  //show the window
  gtk_widget_show (window);

  gtk_main ();
  
  return 0;
  
}
