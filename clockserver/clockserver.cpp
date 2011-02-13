#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <vector>
#include <math.h>

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

#include "../common/helper.h"

using namespace std;


//define variables
const char *configFileName;

unsigned server_count = 1;

//this function loads the config file so that the server parameters don't need to be added every time
void loadConfigFile()
{
	conf configuration = parseconf(configFileName);
	char tmp[40] ;

	//number of region servers required
	if (configuration.find("NUMSERVERS") == configuration.end()) {
		cerr << "Config file is missing an entry!" << endl;
		exit(1);
	}
	strcpy(tmp, configuration["NUMSERVERS"].c_str());
	server_count = atoi(tmp);
}

int main(int argc, char **argv) {
	helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	cout<<"Using config file: "<<configFileName<<endl;

  //loadConfigFile();
  WorldInfo worldinfo;
  conf configuration = parseconf(configFileName);
  if(configuration.find("NUMSERVERS") == configuration.end() ||
     configuration.find("TEAMS") == configuration.end() ||
     configuration.find("ROBOTS_PER_TEAM") == configuration.end()) {
    cerr << "Config file is missing an entry!" << endl;
    return 1;
  }
  server_count = strtol(configuration["NUMSERVERS"].c_str(), NULL, 10);

  {                             // Create initial world state
    unsigned teams = atoi(configuration["TEAMS"].c_str());
    unsigned robots_per_team = atoi(configuration["ROBOTS_PER_TEAM"].c_str());
    unsigned id = 0, region = 0;
    for(unsigned team = 0; team < teams; ++team) {
      for(unsigned robot = 0; robot < robots_per_team; ++robot) {
        RobotInfo *i = worldinfo.add_robot();
        i->set_id(id);
        i->set_region(region);
        i->set_team(team);
        ++id;
        region = (region + 1) % server_count;
      }
    }
  }

  // Disregard SIGPIPE so we can handle things normally
  signal(SIGPIPE, SIG_IGN);

  int sock = net::do_listen(CLOCK_PORT);
  int controllerSock = net::do_listen(CONTROLLERS_PORT);
  int pngSock = net::do_listen(PNG_VIEWER_PORT);
  net::set_blocking(sock, false);
  net::set_blocking(controllerSock, false);
  net::set_blocking(pngSock, false);

  int epoll = epoll_create(server_count);
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(sock);
    close(controllerSock);
    close(pngSock);
    return 1;
  }

  struct epoll_event event;
  helper::connection listenconn(sock, helper::connection::REGION_LISTEN),
    controllerlistenconn(controllerSock, helper::connection::CONTROLLER_LISTEN),
    pnglistenconn(pngSock, helper::connection::PNGVIEWER_LISTEN);
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
  event.data.ptr = &pnglistenconn;
  if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, pngSock, &event)) {
    perror("Failed to add controller listen socket to epoll");
    close(sock);
    close(controllerSock);
    return 1;
  }
  
  vector<helper::connection*> regions, controllers, pngviewers;
  size_t maxevents = 1 + server_count;
  struct epoll_event *events = new struct epoll_event[maxevents];
  size_t connected = 0, ready = 0;
  RegionInfo regioninfo;
  TimestepDone tsdone;
  TimestepUpdate timestep;
  unsigned long long step = 0;
  timestep.set_timestep(step++);
  time_t lastSecond = time(NULL);
  int timeSteps = 0;
  unsigned regionId = 0;
  
  long totalpersecond = 0, number = 0;
  long values[1000];
  long freeval = 0;
  int second = 0;

  cout << "Listening for connections." << endl;
  while(true) {    
    int eventcount = epoll_wait(epoll, events, maxevents, -1);
    if(eventcount < 0) {
      perror("Failed to wait on sockets");
      break;
    }

    for(size_t i = 0; i < (unsigned)eventcount; ++i) {
    	helper::connection *c = (helper::connection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case helper::connection::REGION:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          try {          
            if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_TIMESTEPDONE:
            {
              tsdone.ParseFromArray(buffer, len);
              ++ready;
              break;
            }
            case MSG_REGIONINFO:
            {
              RegionInfo *region = worldinfo.add_region();
              region->ParseFromArray(buffer, len);
              region->set_address(c->addr);

              for(vector<helper::connection*>::iterator i = controllers.begin();
                  i != controllers.end(); ++i) {
                (*i)->queue.push(MSG_REGIONINFO, *region);

                event.events = EPOLLOUT;
                event.data.ptr = *i;
                epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
              }

              for(vector<helper::connection*>::iterator i = pngviewers.begin();
                  i != pngviewers.end(); ++i) {
                (*i)->queue.push(MSG_REGIONINFO, *region);
            
                event.events = EPOLLOUT;
                event.data.ptr = *i;
                epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
              }
              
              break;
            }
            
              default:
              cerr << "Unexpected readable socket message! Type:" << type << endl;
            }
              
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
              cout << timeSteps << " timesteps/second. second: " << second++ << endl;
              //do some stats calculations
              totalpersecond += timeSteps;
              number++;
              values[freeval++] = (long)timeSteps;
              long avg = (totalpersecond / number);
              cout << avg << " timesteps/second on average" <<endl;
              //calc std dev
              long stddev =0;
              for(int k = 0; k < freeval; k++)
                stddev += (values[k] - avg)*(values[k] - avg);
              stddev /= freeval;
              stddev = sqrt(stddev);
              cout << "Standard Deviation: " << stddev << endl << endl;
              ////////////////////////////
              timeSteps = 0;
              lastSecond = time(NULL);
            }
            timeSteps++;
            
            // All servers are ready, prepare to send next step
            ready = 0;
            timestep.set_timestep(step++);
            // Send to regions
            for(vector<helper::connection*>::iterator i = regions.begin();
                i != regions.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
              event.events = EPOLLOUT;
              event.data.ptr = *i;
              epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
            }
            // Send to controllers
            for(vector<helper::connection*>::iterator i = controllers.begin();
                i != controllers.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
              event.events = EPOLLOUT;
              event.data.ptr = *i;
              epoll_ctl(epoll, EPOLL_CTL_MOD, (*i)->fd, &event);
            }
          }
          break;
        }

        case helper::connection::REGION_LISTEN:
        {
          // Accept a new region server
          struct sockaddr_storage addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept region");
          }
          net::set_blocking(fd, false);

          helper::connection *newconn = new helper::connection(fd, helper::connection::REGION);
          newconn->addr = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
          regions.push_back(newconn);

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

        case helper::connection::CONTROLLER_LISTEN:
        {
          // Accept a new controller
          struct sockaddr_storage addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept controller");
          }
          net::set_blocking(fd, false);

          helper::connection *newconn = new helper::connection(fd, helper::connection::CONTROLLER);
          newconn->addr = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
          controllers.push_back(newconn);
          
          newconn->queue.push(MSG_WORLDINFO, worldinfo);

          event.events = EPOLLOUT;
          event.data.ptr = newconn;
          if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
            perror("Failed to add controller socket to epoll");
            return 1;
          }

          cout << "Got controller connection." << endl;
          break;
        }
        case helper::connection::PNGVIEWER_LISTEN:
        {
          // Accept a new pngviewer
          struct sockaddr_storage addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept png viewer");
          }
          net::set_blocking(fd, false);

          helper::connection *newconn = new helper::connection(fd, helper::connection::PNGVIEWER);
          newconn->addr = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
          pngviewers.push_back(newconn);

          for(size_t i = 0; i < (unsigned)worldinfo.region_size(); ++i) {
            newconn->queue.push(MSG_REGIONINFO, worldinfo.region(i));
          }

          event.events = EPOLLOUT;
          event.data.ptr = newconn;
          if(0 > epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
            perror("Failed to add png viewer socket to epoll");
            return 1;
          }

          cout << "Got png viewer connection." << endl;
          break;
        }

        default:
          cerr << "Internal error: Got unexpected readable event!" << endl;
          break;
        }
      } else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case helper::connection::PNGVIEWER:
        case helper::connection::CONTROLLER:
          if(c->queue.doWrite()) {
            // If the queue is empty, we don't care if this is writable
            event.events = 0;
            event.data.ptr = c;
            epoll_ctl(epoll, EPOLL_CTL_MOD, c->fd, &event);
          }
          break;
        case helper::connection::REGION:
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
  for(vector<helper::connection*>::iterator i = controllers.begin();
      i != controllers.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  for(vector<helper::connection*>::iterator i = regions.begin();
      i != regions.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  close(sock);
  close(controllerSock);

  return 0;
}
