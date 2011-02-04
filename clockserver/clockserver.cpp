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
char configFileName [30] = "config";

int max_controllers = 10;
unsigned server_count = 1;
int *servers;

int *controllers; //Ben, you killed my controller handling code in the merge
int freeController = 0;
int sock, controllerSock;



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
	
	//create a read buffer. No line should be longer than 200 chars long.
	char readBuffer [200];
	char * token;
	
	if (fileHandle != NULL)
	{
		while(fgets (readBuffer , sizeof(readBuffer) , fileHandle) != 0)
		{	
			token = strtok(readBuffer, " \n");
			
			//if it's a REGION WIDTH definition...
			if(strcmp(token, "NUMSERVERS") == 0){
				token = strtok(NULL, " \n");
				server_count = atoi(token);
				printf("NUMSERVERS: %d\n", server_count);
			}
			
		}
		
		fclose (fileHandle);
	}else
		printf("Error: Cannot open config file %s\n", configFileName);
}


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

  vector<int> servers(server_count, -1);
  vector<MessageReader> readers;
  vector<MessageWriter> writers;
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
      if(events[i].data.u32 == 0) {
        // Accept a new region server
        int fd = accept(sock, NULL, NULL);
        net::set_blocking(fd, false);
        servers[connected] = fd;
        event.events = EPOLLIN;
        event.data.u32 = connected+1;
        if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
          perror("Failed to add region server socket to epoll");
          return 1;
        }

        readers.push_back(MessageReader(fd));
        writers.push_back(MessageWriter(fd));

        cout << "Region server " << connected << " connected!" << endl;

        ++connected;
      } else if(events[i].events & EPOLLIN) {
        MessageType type;
        size_t len;
        const void *buffer;
        size_t index = events[i].data.u32 - 1;
        try {
          if(readers[index].doRead(&type, &len, &buffer)) {
            tsdone.ParseFromArray(buffer, len);
            cout << "Region server " << index << " done." << endl;
            ++ready;
          }
        } catch(EOFError e) {
          cerr << "Region server " << index
               << " disconnected!  Shutting down." << endl;
          return 1;
        } catch(SystemError e) {
          cerr << "Error reading from region server " << index
               << ": " << e.what() << ".  Shutting down." << endl;
          return 1;
        }
        
        if(ready == connected && connected == server_count) {
          cout << "All servers done, sending timestep " << step << "." << endl;
          // All servers are ready, prepare to send next step
          ready = 0;
          tsupdate.set_timestep(step++);
          for(size_t j = 0; j < connected; ++j) {
            writers[j].init(TIMESTEPUPDATE, &tsupdate);
            
            event.events = EPOLLOUT;
            event.data.u32 = j+1;
            epoll_ctl(epoll, EPOLL_CTL_MOD, servers[j], &event);
          }
        }
      } else if(events[i].events & EPOLLOUT) {
        size_t index = events[i].data.u32 - 1;
        if(writers[index].doWrite()) {
          // We're done writing for this server
          cout << "Send complete to region server " << index << "." << endl;
          event.events = EPOLLIN;
          event.data.u32 = index+1;
          epoll_ctl(epoll, EPOLL_CTL_MOD, servers[index], &event);
        }
      }
    }
  }

  // Clean up
  close(epoll);
  for(size_t i = 0; i < connected; ++i) {
    if(i > 0) {
      shutdown(servers[i], SHUT_RDWR);
      close(servers[i]);
    }
  }
  return 0;
}
