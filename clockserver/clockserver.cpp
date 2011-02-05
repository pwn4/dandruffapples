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

#include "../common/timestep.pb.h"

#include "../common/except.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagereader.h"
#include "../common/messagewriter.h"

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
  enum {
    REGION,
    CONTROLLER
  } type;
  int fd;
  MessageReader reader;
  MessageWriter writer;

  connection(int fd_) : fd(fd_), reader(fd_), writer(fd_) {}
};

int main(int argc, char **argv) {
  parseArguments(argc, argv);

  loadConfigFile();

  int sock = net::do_listen(CLOCK_PORT);
  net::set_blocking(sock, false);

  int epoll = epoll_create(server_count);
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(sock);
    return 1;
  }

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.u32 = 0;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event)) {
    perror("Failed to add listen socket to epoll");
  }

  
  vector<connection> connections;
  size_t maxevents = 1 + server_count;
  struct epoll_event *events = new struct epoll_event[maxevents];
  size_t connected = 0, ready = 0;
  TimestepDone tsdone;
  TimestepUpdate tsupdate;
  unsigned long long step = 0;

  cout << "Listening for connections." << endl;
  while(true) {    
    int eventcount = epoll_wait(epoll, events, maxevents, -1);
    if(eventcount < 0) {
      perror("Failed to wait on sockets");
      break;
    }

    for(size_t i = 0; i < (unsigned)eventcount; ++i) {
      connection *c = (connection*)events[i].data.ptr;
      if(events[i].data.u32 == 0) {
        // Accept a new region server
        int fd = accept(sock, NULL, NULL);
        net::set_blocking(fd, false);

        connection c(fd);
        c.type = connection::REGION;
        connections.push_back(c);

        event.events = EPOLLIN;
        event.data.ptr = &connections.back();
        if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
          perror("Failed to add region server socket to epoll");
          return 1;
        }

        cout << "Got region server connection." << endl;

        ++connected;
      } else if(events[i].events & EPOLLIN &&
                c->type == connection::REGION) {
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
          cout << " done!" << endl
               << "Sending timestep " << step << flush;
          // All servers are ready, prepare to send next step
          ready = 0;
          tsupdate.set_timestep(step++);
          for(vector<connection>::iterator i = connections.begin();
              i != connections.end(); ++i) {
            i->writer.init(TIMESTEPUPDATE, &tsupdate);
            
            event.events = EPOLLOUT;
            event.data.ptr = &*i;
            epoll_ctl(epoll, EPOLL_CTL_MOD, i->fd, &event);
          }
        }
      } else if(events[i].events & EPOLLOUT) {
        if(c->writer.doWrite()) {
          // We're done writing for this server
          cout << "." << flush;
          event.events = EPOLLIN;
          event.data.ptr = c;
          epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
        }
      }
    }
  }

  // Clean up
  close(epoll);
  for(vector<connection>::iterator i = connections.begin();
      i != connections.end(); ++i) {
    shutdown(i->fd, SHUT_RDWR);
    close(i->fd);
  }

  return 0;
}
