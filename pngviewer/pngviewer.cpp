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
char configFileName [30] = "config";
int clockfd, listenfd, clientfd;

//Config variables
char clockip [40] = "127.0.0.1";

//this function parses any minimal command line arguments and uses their values
void parseArguments(int argc, char* argv[])
{
	//loop through the arguments
	for(int i = 0; i < argc; i++)
	{
		//if it's a configuration file name...
		if(strcmp(argv[i], "-c") == 0)
		{
			strcpy(configFileName, argv[i+1]);
            
			printf("Using config file: %s\n", configFileName);
			
			i++; //increment the loop counter for one argument
		}
	}
}


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

        cairo_surface_t *image;

        image = cairo_image_surface_create_from_png("rose.png");

        cairo_t *cr;

        cr = gdk_cairo_create (widget->window);

        cairo_set_source_surface(cr, image, 10, 10);
        cairo_paint(cr);

        cairo_destroy(cr);

        return FALSE;
}


//main method
int main(int argc, char* argv[])
{
  //parse command line arguments and load the config file
	parseArguments(argc, argv);
  loadConfigFile();
  
  //connect to the clock server
  clockfd = net::do_connect(clockip, PNG_VIEWER_PORT);
  cout << "Connected to Clock Server" << endl;
  
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
  
  //set the border width of the window.
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
 
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "PNGViewer");
  gtk_window_set_default_size(GTK_WINDOW(window), 900, 900); 
  gtk_widget_set_app_paintable(window, TRUE);

  gtk_widget_show_all(window);

  gtk_main();


  
  //show the window
  gtk_widget_show (window);

  gtk_main ();
  
  return 0;
  
  
  
  

  
  
  /*
  
  

	TimestepUpdate timestep;
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  MessageReader reader(clockfd);
  MessageType type;
  size_t len;
  const void *buffer;

  try {
		while(true) {
		  for(bool complete = false; !complete;) {
		    complete = reader.doRead(&type, &len, &buffer);
		  }
		  switch (type) {
				case MSG_REGIONINFO:
				{
				//we got regionserver information
					regioninfo.ParseFromArray(buffer, len);
					cout << "Received MSG_REGIONINFO update!" << regioninfo.address() << " " << regioninfo.port() << endl;
					struct in_addr addr;
					addr.s_addr = regioninfo.address();
					int fd = net::do_connect(addr, regioninfo.port());
					cout << "Connected to region server!" << endl;
					
					break;
				}
				case MSG_REGIONRENDER:
				{
					cout << "Received MSG_REGIONRENDER update!" << endl;
					break;
				}
				default:
				{
				  cout << "Unknown message!" << endl;
				  break;
				}
		  }
		}
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }   */
}
