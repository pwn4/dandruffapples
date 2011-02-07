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

using namespace std;

/////////////////Variables and Declarations/////////////////
char configFileName [30] = "config";

//Config variables
char clockip [40] = "127.0.0.1";

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
  if(configuration.find("CLOCKIP") == configuration.end()) {
    cerr << "Config file is missing an entry!" << endl;
    exit(1);
  }
  strcpy(clockip, configuration["CLOCKIP"].c_str());
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
    LISTEN,
    CLOCK,
    CONTROLLER,
    REGION,
    FILE
  } type;
  int fd;
  MessageReader reader;
  MessageQueue queue;

  connection(int fd_) : type(UNSPECIFIED), fd(fd_), reader(fd_), queue(fd_) {}
  connection(int fd_, Type type_) : type(type_), fd(fd_), reader(fd_), queue(fd_) {}
};

//the main function
void run() {

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

  //handle logging to file initializations
  string logName=helper::getNewName("/tmp/antix_log");
  int logfd = open(logName.c_str(), O_WRONLY | O_CREAT);
  PuckStack puckstack;
  ServerRobot serverrobot;
  puckstack.set_x(1);
  puckstack.set_y(1);
  puckstack.set_stacksize(1);
  serverrobot.set_id(2);

  TimestepUpdate timestep;
  TimestepDone tsdone;
  tsdone.set_done(true);
  MessageWriter writer(clockfd);
  MessageReader reader(clockfd);
  MessageType type;
  size_t len;
  const void *buffer;
  int timeSteps = 0;
  time_t lastSecond = time(NULL);

  try {
    cout << "Notifying clock server that our init's complete..." << flush;
    while(true) {
    
      //check if its time to output
      if(time(NULL) > lastSecond)
      {
        cout << timeSteps << " timesteps/second." << endl;
        timeSteps = 0;
        lastSecond = time(NULL);
      }


      writer.init(MSG_TIMESTEPDONE, tsdone);
      for(bool complete = false; !complete;) {
        complete = writer.doWrite();
      }

      for(bool complete = false; !complete;) {
        complete = reader.doRead(&type, &len, &buffer);
      }
      
      if(type == MSG_TIMESTEPUPDATE)
      {
        timeSteps++;
        timestep.ParseFromArray(buffer, len);
      }
      
    }
  } catch(EOFError e) {
    cout << " clock server disconnected, shutting down." << endl;
  } catch(SystemError e) {
    cerr << " error performing network I/O: " << e.what() << endl;
  }

  // Clean up
  shutdown(clockfd, SHUT_RDWR);
  close(clockfd);
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
