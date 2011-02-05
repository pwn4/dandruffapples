#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#include "../common/timestep.pb.h"
#include "../common/worldinfo.pb.h"

#include "../common/except.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/parseconf.h"

using namespace std;


//define variables
char configFileName[30] = "config";

unsigned server_count = 1;

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


//this function loads the config file so that the server parameters don't need to be added every time
void loadConfigFile()
{
	//open the config file
	FILE * fileHandle;
	fileHandle = fopen (configFileName,"r");
  if(0 > fileHandle) {
    perror("Couldn't open config file");
    return;
  }
	
	char *buffer = NULL, *endptr = NULL;
  size_t len;
  while(0 < getdelim(&buffer, &len, ' ', fileHandle)) {	
    if(!strcmp(buffer, "NUMSERVERS ")) {
      if(0 > getdelim(&buffer, &len, '\n', fileHandle)) {
        cerr << "Couldn't read value for NUMSERVERS from config file." << endl;
        break;
      }
      server_count = strtol(buffer, &endptr, 10);
      if(endptr == buffer) {
        cerr << "Value for NUMSERVERS in config file is not a number!" << endl;
        break;
      }
      printf("NUMSERVERS: %d\n", server_count);
    }
  }
		
  fclose(fileHandle);
}

struct connection {
  enum Type {
    UNSPECIFIED,
    REGION_LISTEN,
    CONTROLLER_LISTEN,
    REGION,
    CONTROLLER
  } type;
  
  enum State {
    INIT,
    RUN
  } state;
  
  int fd;
  MessageReader reader;
  MessageQueue queue;

  connection(int fd_) : type(UNSPECIFIED), state(INIT), fd(fd_), reader(fd_), queue(fd_) {}
  connection(int fd_, Type type_) : type(type_), state(INIT), fd(fd_), reader(fd_), queue(fd_) {}
};

int main(int argc, char **argv) {
  parseArguments(argc, argv);

  //loadConfigFile();
  conf configuration = parseconf(configFileName);
  if(configuration.find("NUMSERVERS") != configuration.end())
    server_count = strtol(configuration["NUMSERVERS"].c_str(), NULL, 10);

  // Disregard SIGPIPE so we can handle things normally
  signal(SIGPIPE, SIG_IGN);

  int sock = net::do_listen(CLOCK_PORT);
  int controllerSock = net::do_listen(CONTROLLERS_PORT);
  net::set_blocking(sock, false);
  net::set_blocking(controllerSock, false);

  int epoll = epoll_create(server_count);
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(sock);
    close(controllerSock);
    return 1;
  }

  struct epoll_event event;
  connection listenconn(sock, connection::REGION_LISTEN),
    controllerlistenconn(controllerSock, connection::CONTROLLER_LISTEN);
  event.events = EPOLLIN;
  event.data.ptr = &listenconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event)) {
    perror("Failed to add listen socket to epoll");
    close(sock);
    close(controllerSock);
    return 1;
  }
  event.data.ptr = &controllerlistenconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, controllerSock, &event)) {
    perror("Failed to add controller listen socket to epoll");
    close(sock);
    close(controllerSock);
    return 1;
  }
  
  vector<connection*> connections;
  size_t maxevents = 1 + server_count;
  struct epoll_event *events = new struct epoll_event[maxevents];
  size_t connected = 0, ready = 0;
  TimestepDone tsdone;
  TimestepUpdate timestep;
  unsigned long long step = 0;
  timestep.set_timestep(step++);
  time_t lastSecond = time(NULL);
  int timeSteps = 0;

  cout << "Listening for connections." << endl;
  while(true) {    
    int eventcount = epoll_wait(epoll, events, maxevents, -1);
    if(eventcount < 0) {
      perror("Failed to wait on sockets");
      break;
    }

    for(size_t i = 0; i < (unsigned)eventcount; ++i) {
      connection *c = (connection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case connection::REGION:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          try {
            if(c->reader.doRead(&type, &len, &buffer)) {
              tsdone.ParseFromArray(buffer, len);
              ++ready;
            }
          } catch(EOFError e) {
            cerr << "Region server disconnected!  Shutting down." << endl;
            return 1;
          } catch(SystemError e) {
            cerr << "Error reading from region server: "
                 << e.what() << ".  Shutting down." << endl;
            return 1;
          }
        
          if(ready == connected && connected == server_count) {
            //check if its time to output
            if(time(NULL) > lastSecond)
            {
              cout << timeSteps << " timesteps/second." << endl;
              timeSteps = 0;
              lastSecond = time(NULL);
            }
            timeSteps++;
            
            // All servers are ready, prepare to send next step
            ready = 0;
            timestep.set_timestep(step++);
            msg_ptr update(new TimestepUpdate(timestep));
            for(vector<connection*>::iterator i = connections.begin();
                i != connections.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, update);
              event.events = EPOLLOUT;
              event.data.ptr = *i;
              epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
            }
          }
          break;
        }

        case connection::REGION_LISTEN:
        {
          // Accept a new region server
          int fd = accept(c->fd, NULL, NULL);
          if(fd < 0) {
            throw SystemError("Failed to accept region");
          }
          net::set_blocking(fd, false);

          connection *newconn = new connection(fd, connection::REGION);
          connections.push_back(newconn);

          event.events = EPOLLIN;
          event.data.ptr = newconn;
          if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
            perror("Failed to add region server socket to epoll");
            return 1;
          }

          cout << "Got region server connection." << endl;

          ++connected;
          break;
        }

        case connection::CONTROLLER_LISTEN:
        {
          // Accept a new region server
          int fd = accept(c->fd, NULL, NULL);
          if(fd < 0) {
            throw SystemError("Failed to accept controller");
          }
          net::set_blocking(fd, false);

          connection *newconn = new connection(fd, connection::CONTROLLER);
          connections.push_back(newconn);

          event.events = EPOLLOUT;
          event.data.ptr = newconn;
          if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
            perror("Failed to add region server socket to epoll");
            return 1;
          }
          break;
        }

        default:
          cerr << "Internal error: Got unexpected readable event!" << endl;
          break;
        }
      } else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case connection::CONTROLLER:
          if(c->state == connection::INIT) {
            // Begin initialization message send
          }
          if(c->queue.doWrite()) {
            event.events = 0;
            event.data.ptr = c;
            epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
          }
          break;
        case connection::REGION:
          if(c->queue.doWrite()) {
            // We're done writing for this server
            event.events = EPOLLIN;
            event.data.ptr = c;
            epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
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
  close(epoll);
  for(vector<connection*>::iterator i = connections.begin();
      i != connections.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  close(sock);
  close(controllerSock);

  return 0;
}
