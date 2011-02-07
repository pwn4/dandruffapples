/*/////////////////////////////////////////////////////////////////////////////////////////////////
Regionserver program
This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
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

using namespace std;
using namespace Magick;
/////////////////Variables and Declarations/////////////////
char configFileName [30] = "config";

//Config variables
char clockip [40] = "127.0.0.1";

int controllerPort = CONTROLLERS_PORT;
int pngviewerPort = PNG_VIEWER_PORT;
int regionPort = REGIONS_PORT;

////////////////////////////////////////////////////////////


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
  
  //controller listening port
  if(configuration.find("CTRLPORT") != configuration.end()) {
    controllerPort = strtol(configuration["CTRLPORT"].c_str(), NULL, 10);
  }
  
  //region server listening port
  if(configuration.find("REGPORT") != configuration.end()) {
    regionPort = strtol(configuration["REGPORT"].c_str(), NULL, 10);
  }
  
  //png viewer listening port
  if(configuration.find("PNGPORT") != configuration.end()) {
    pngviewerPort = strtol(configuration["PNGPORT"].c_str(), NULL, 10);
  }
  
}


char *parse_port(char *input) {
  size_t input_len = strlen(input);
  char *port;
  
  for(port = input; *port != ':' && (port - input) < input_len; ++port);
  
  if((port - input) == input_len) {
    return NULL;
  } else {
    // Split the string
    *port = '\0';
    ++port;
    // Strip newline
    char *end;
    for(end = port; *end != '\n'; ++end);
    if(end == port) {
      return NULL;
    } else {
      *end = '\0';
      return port;
    }
  }
}

//specifies an object to work with sockets and file IO
struct connection {
  enum Type {
    UNSPECIFIED,
    REGION_LISTEN,
    CONTROLLER_LISTEN,
    PNGVIEWER_LISTEN,
    CLOCK,
    CONTROLLER,
    REGION,
    PNGVIEWER,
    FILE
  } type;
  int fd;
  MessageReader reader;
  MessageQueue queue;

  connection(int fd_) : type(UNSPECIFIED), fd(fd_), reader(fd_), queue(fd_) {}
  connection(int fd_, Type type_) : type(type_), fd(fd_), reader(fd_), queue(fd_) {}
};

Blob handleWorldImage()
{
	Blob blob;
	Image regionPiece("320x320", "white");
	regionPiece.magick("png");

	int x,y;

	for( int i=0;i<10;i++ )
	{
		x=1 + rand()%318;
		y=1 + rand()%318;

		//make the pixel more visible by drawing more pixels around it
		regionPiece.pixelColor(x, y, Color("black"));
		regionPiece.pixelColor(x+1, y, Color("black"));
		regionPiece.pixelColor(x-1, y, Color("black"));
		regionPiece.pixelColor(x, y+1, Color("black"));
		regionPiece.pixelColor(x, y-1, Color("black"));
	}
	regionPiece.write(&blob);

	return blob;
}

//the main function
void run() {

  // Disregard SIGPIPE so we can handle things normally
  signal(SIGPIPE, SIG_IGN);

  //connect to the clock server
  cout << "Connecting to clock server..." << flush;
  int clockfd = net::do_connect(clockip, CLOCK_PORT);
  if(0 > clockfd) {
    perror(" failed to connect to clock server");
    exit(1);;
  } else if(0 == clockfd) {
    cerr << " invalid address: " << clockip << endl;
    exit(1);;
  }
  cout << " done." << endl;


  //listen for controller connections
  int controllerfd = net::do_listen(controllerPort);
  net::set_blocking(controllerfd, false);

  //listen for region server connections
  int regionfd = net::do_listen(regionPort);
  net::set_blocking(regionfd, false);
  
  //listen for PNG viewer connections
  int pngfd = net::do_listen(pngviewerPort);
  net::set_blocking(pngfd, false);

  //create epoll
  int epoll = epoll_create(15); //9 adjacents, the clock, and a few controllers
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    exit(1);
  }
  
  // Add clock and client sockets to epoll
  connection clockconn(clockfd, connection::CLOCK),
    controllerconn(controllerfd, connection::CONTROLLER_LISTEN),
    regionconn(regionfd, connection::REGION_LISTEN),
    pngconn(pngfd, connection::PNGVIEWER_LISTEN);
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = &clockconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, clockfd, &event)) {
    perror("Failed to add controller socket to epoll");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    exit(1);
  }
  event.data.ptr = &controllerconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, controllerfd, &event)) {
    perror("Failed to add controller socket to epoll");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    exit(1);
  }
  event.data.ptr = &regionconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, regionfd, &event)) {
    perror("Failed to add region socket to epoll");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    exit(1);
  }
  event.data.ptr = &pngconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, pngfd, &event)) {
    perror("Failed to add pngviewer socket to epoll");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    exit(1);
  }

  //handle logging to file initializations
  string logName=helper::getNewName("/tmp/antix_log");
  int logfd = open(logName.c_str(), O_WRONLY | O_CREAT);
  PuckStack puckstack;
  ServerRobot serverrobot;
  puckstack.set_x(1);
  puckstack.set_y(1);
  puckstack.set_stacksize(1);
  serverrobot.set_id(2);

  RegionRender png;
  //I think this is what we want
  Blob blob=handleWorldImage();
  png.set_image(blob.data(), blob.length());

  //server variables
  TimestepUpdate timestep;
  TimestepDone tsdone;
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  tsdone.set_done(true);
  
  MessageWriter writer(clockfd);
  MessageReader reader(clockfd);
  MessageType type;
  
  int timeSteps = 0;
  time_t lastSecond = time(NULL);
  vector<connection*> controllers;
  vector<connection*> pngviewers;
  vector<connection*> borderRegions;  


  #define MAX_EVENTS 128
  struct epoll_event events[MAX_EVENTS];
  //send the timestepdone packet to tell the clock server we're ready
  writer.init(MSG_TIMESTEPDONE, tsdone);
  for(bool complete = false; !complete;) {
    complete = writer.doWrite();
  }
  
  //enter the main loop
  while(true) {
  
    //check if its time to output
    if(time(NULL) > lastSecond)
    {
      cout << timeSteps << " timesteps/second." << endl;
      timeSteps = 0;
      lastSecond = time(NULL);
    }

    //wait on epoll
    int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
    if(0 > eventcount) {
      perror("Failed to wait on sockets");
      break;
    }

    //check our events that were triggered
    for(size_t i = 0; i < (unsigned)eventcount; i++) {
      connection *c = (connection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case connection::CLOCK:
        {
          //we get a message from the clock server
          MessageType type;
          size_t len;
          const void *buffer;
          if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_WORLDINFO:
            {
              worldinfo.ParseFromArray(buffer, len);
              cout << "Got world info." << endl;
              break;
            }
            case MSG_REGIONINFO:
            {
              regioninfo.ParseFromArray(buffer, len);
              cout << "Got region info." << endl;
              break;
            }
            case MSG_TIMESTEPUPDATE:
            {
              timestep.ParseFromArray(buffer, len);
              
              //DO TIMESTEP STUFF HERE/////////
              timeSteps++;
              
              //END TIMESTEP CALCULATIONS//////


              //Respond with done message
              msg_ptr update(new TimestepDone(tsdone));
              c->queue.push(MSG_TIMESTEPUPDATE, update);
                event.events = EPOLLIN | EPOLLOUT;
                event.data.ptr = c;
                epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
              break;
            }
              default:
              cerr << "Unexpected readable socket!" << endl;
            }
          }
        }
        case connection::CONTROLLER:
        {
          
          break;
        }
        case connection::REGION:
        {
        
          break;
        }
        case connection::REGION_LISTEN:
        {
        
          break;
        }
        case connection::CONTROLLER_LISTEN:
        {
        
          break;
        }
        case connection::PNGVIEWER_LISTEN:
        {
        
          break;
        }
        
        default:
          cerr << "Internal error: Got unexpected readable event!" << endl;
          break;
        }     
      }else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case connection::CLOCK:
          if(c->queue.doWrite()) {
            //write data to the clock
            event.events = EPOLLIN;
            event.data.ptr = c;
            epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
          }
          break;
        case connection::PNGVIEWER:
        case connection::CONTROLLER:
        case connection::REGION:

        default:
          cerr << "Unexpected writable socket!" << endl;
          break;
        }
      }
    }
  }

  // Clean up
  shutdown(clockfd, SHUT_RDWR);
  close(clockfd);
  shutdown(controllerfd, SHUT_RDWR);
  close(controllerfd);
  shutdown(regionfd, SHUT_RDWR);
  close(regionfd);
  shutdown(pngfd, SHUT_RDWR);
  close(pngfd);
}


//this is the main loop for the server
int main(int argc, char* argv[])
{
	//Print a starting message
	printf("--== Region Server Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Server Initializing ...\n");
	
	parseArguments(argc, argv);
	
	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Server Running!\n");
	
	run();
	
	printf("Server Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
