/*/////////////////////////////////////////////////////////////////////////////////////////////////
Regionserver program
This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
//////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

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
#include <math.h>

#include <google/protobuf/message_lite.h>

#include "../common/timestep.pb.h"
#include "../common/net.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
#include "../common/messagewriter.h"
#include "../common/worldinfo.pb.h"
#include "../common/regionrender.pb.h"

#include "../common/ports.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/net.h"
#include "../common/except.h"
#include "../common/parseconf.h"
#include "../common/timestep.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"

#include "areaengine.h"

#include "../common/helper.h"
#include <Magick++.h>

using namespace std;
using namespace Magick;
/////////////////Variables and Declarations/////////////////
const char *configFileName;

//Config variables
char clockip [40] = "127.0.0.1";

int controllerPort = CONTROLLERS_PORT;
int pngviewerPort = PNG_VIEWER_PORT;
int regionPort = REGIONS_PORT;

////////////////////////////////////////////////////////////

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
  signed int input_len = (int)strlen(input);
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
  srand(time(NULL));

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

  //create a new file for logging
  string logName=helper::getNewName("/tmp/"+helper::defaultLogName);
  int logfd = open(logName.c_str(), O_WRONLY | O_CREAT, 0644 );

  if(logfd<0)
  {
	  perror("Failed to create log file");
	  exit(1);
  }

  //create epoll
  int epoll = epoll_create(16); //9 adjacents, log file, the clock, and a few controllers
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(controllerfd);
    close(regionfd);
    close(pngfd);
    close(clockfd);
    close(logfd);
    exit(1);
  }
  
  // Add clock and client sockets to epoll
  net::EpollConnection clockconn(epoll, EPOLLIN, clockfd, net::connection::CLOCK),
    controllerconn(epoll, EPOLLIN, controllerfd, net::connection::CONTROLLER_LISTEN),
    regionconn(epoll, EPOLLIN, regionfd, net::connection::REGION_LISTEN),
    pngconn(epoll, EPOLLIN, pngfd, net::connection::PNGVIEWER_LISTEN);
  
  //handle logging to file initializations
  PuckStack puckstack;
  ServerRobot serverrobot;
  puckstack.set_x(1);
  puckstack.set_y(1);

  RegionRender png;
  Blob blob;

  //server variables
  MessageWriter logWriter(logfd);
  TimestepUpdate timestep;
  TimestepDone tsdone;
  tsdone.set_done(true);
  WorldInfo worldinfo;
  RegionInfo regioninfo;
  //Region Area Variables (should be set by clock server)
  int regionSideLen = 2000;
  int robotDiameter = 4;
  int minElementSize =  4;
  double viewDistance = 20;
  double viewAngle = 360;
  double maxSpeed = 4;
  AreaEngine* regionarea = new AreaEngine(robotDiameter, regionSideLen, minElementSize, viewDistance, viewAngle, maxSpeed);
  //create robots for benchmarking!
  int numRobots = 0;
  int wantRobots = 1000;
  //regionarea->AddRobot(10, 0, 1, 0, 0, 0);
  //regionarea->AddRobot(12, 15.8, 1, 0, 0, 0);
  for(int i = robotDiameter; i < regionSideLen-(robotDiameter) && numRobots < wantRobots; i += 2*(robotDiameter))
    for(int j = robotDiameter; j < regionSideLen-(robotDiameter) && numRobots < wantRobots; j += 2*(robotDiameter))
        regionarea->AddRobot(numRobots++, i, j, 0, 0, 0);

    cout << numRobots << " robots created." << endl;
  MessageWriter writer(clockfd);
  MessageReader reader(clockfd);
  int timeSteps = 0;
  time_t lastSecond = time(NULL);
  vector<net::EpollConnection*> controllers;
  vector<net::EpollConnection*> pngviewers;
  vector<net::EpollConnection*> borderRegions;

  //send port listening info (IMPORTANT)
  //add listening ports
  RegionInfo info;
  info.set_address(0);
  info.set_id(0);
  info.set_regionport(regionPort);
  info.set_renderport(pngviewerPort);
  info.set_controllerport(controllerPort);
  clockconn.queue.push(MSG_REGIONINFO, info);
  clockconn.set_writing(true);

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
    	net::EpollConnection *c = (net::EpollConnection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case net::connection::CLOCK:
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

              regionarea->Step();
              timeSteps++;  //Note: only use this for this temp stat taking. use regionarea->curStep for syncing
              
              
/*
              if(timestep.timestep() % 200 == 0) {
                // Only generate an image for one in 200 timesteps
                blob = handleWorldImage();
                png.set_image(blob.data(), blob.length());
                png.set_timestep(timestep.timestep());
                for(vector<net::EpollConnection*>::iterator i = pngviewers.begin();
                    i != pngviewers.end(); ++i) {
                  (*i)->queue.push(MSG_REGIONRENDER, png);
                  (*i)->set_writing(true);
                }
              }
#ifdef ENABLE_LOGGING
              logWriter.init(MSG_TIMESTEPUPDATE, timestep);
              logWriter.doWrite();

              for(int i = 0;i <50; i++) {
            	  serverrobot.set_id(rand()%1000+1);
            	  logWriter.init(MSG_SERVERROBOT, serverrobot);
            	  for(bool complete = false; !complete;) {
            	    complete = logWriter.doWrite();;
            	  }
               }

              for(int i = 0;i <25; i++) {
            	  puckstack.set_stacksize(rand()%1000+1);
            	  logWriter.init(MSG_PUCKSTACK, puckstack);
            	  for(bool complete = false; !complete;) {
            	    complete = logWriter.doWrite();;
            	  }
               }
#endif*/
              //Respond with done message
              c->queue.push(MSG_TIMESTEPDONE, tsdone);
              c->set_writing(true);
              break;
            }
              default:
              cerr << "Unexpected readable socket!" << endl;
            }
          }
        }
        case net::connection::CONTROLLER:
        {
          
          break;
        }
        case net::connection::REGION:
        {
        
          break;
        }
        case net::connection::REGION_LISTEN:
        {
        
          break;
        }
        case net::connection::CONTROLLER_LISTEN:
        {
        
          break;
        }
        case net::connection::PNGVIEWER_LISTEN:
        {
            // Accept a new pngviewer
            int fd = accept(c->fd, NULL, NULL);
            if(fd < 0) {
              throw SystemError("Failed to accept png viewer");
            }
            net::set_blocking(fd, false);

            try {
              net::EpollConnection *newconn = new net::EpollConnection(epoll, EPOLLOUT, fd, net::connection::PNGVIEWER);
              pngviewers.push_back(newconn);
            } catch(SystemError e) {
              cerr << e.what() << endl;
              return;
            }

            cout << "Got png viewer connection." << endl;
            break;
        }
        
        default:
          cerr << "Internal error: Got unexpected readable event of type " <<c->type<< endl;
          break;
        }     
      } else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case net::connection::CONTROLLER:
        
          break;
        case net::connection::REGION:
        
          break;
        case net::connection::CLOCK:
        case net::connection::PNGVIEWER:
          // Perform write
          if(c->queue.doWrite()) {
            // If the queue is empty, we don't care if this is writable
            c->set_writing(false);
          }
          break;
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
  close(logfd);
}


//this is the main loop for the server
int main(int argc, char* argv[])
{
	//Print a starting message
	printf("--== Region Server Software ==-\n");
	
	////////////////////////////////////////////////////
	printf("Server Initializing ...\n");
	
  helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	cout<<"Using config file: "<<configFileName<<endl;

	loadConfigFile();
	////////////////////////////////////////////////////
	
	printf("Server Running!\n");
	
	run();
	
	printf("Server Shutting Down ...\n");
	
	printf("Goodbye!\n");
}
