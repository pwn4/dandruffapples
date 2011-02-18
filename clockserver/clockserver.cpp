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
#include "../common/claimteam.pb.h"

#include "../common/except.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/parseconf.h"

#include "../common/helper.h"

using namespace std;

struct RegionConnection : public net::EpollConnection {
	in_addr_t addr;

	RegionConnection(int epoll, int flags, int fd, Type type) : net::EpollConnection(epoll, flags, fd, type) {}
};

//define variables
const char *configFileName;

unsigned server_count = 1;
int server_rows = 1;
int server_cols = 1;

int main(int argc, char **argv) {
	helper::Config config(argc, argv);
	configFileName=(config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	cout<<"Using config file: "<<configFileName<<endl;

  //loadConfigFile();
  WorldInfo worldinfo;
  conf configuration = parseconf(configFileName);
  if(configuration.find("SERVERROWS") == configuration.end() ||
     configuration.find("SERVERCOLS") == configuration.end() ||
     configuration.find("TEAMS") == configuration.end() ||
     configuration.find("ROBOTS_PER_TEAM") == configuration.end()) {
    cerr << "Config file is missing an entry!" << endl;
    return 1;
  }
  
  //drawing setup
  server_rows = strtol(configuration["SERVERROWS"].c_str(), NULL, 10);
  server_cols = strtol(configuration["SERVERCOLS"].c_str(), NULL, 10);
  server_count = server_rows*server_cols;
  int ** drawGrid = new int*[server_cols];
  for(int i = 0; i < server_cols; i++)
    drawGrid[i] = new int[server_rows];
    

  bool *teamclaimed;
  {                             // Create initial world state
    unsigned teams = atoi(configuration["TEAMS"].c_str());
    unsigned robots_per_team = atoi(configuration["ROBOTS_PER_TEAM"].c_str());
    unsigned id = 0, region = 0;
    teamclaimed = new bool[teams];
    for(unsigned team = 0; team < teams; ++team) {
      teamclaimed[team] = false;
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

  RegionConnection listenconn(epoll, EPOLLIN, sock, RegionConnection::REGION_LISTEN),
    controllerlistenconn(epoll, EPOLLIN, controllerSock, RegionConnection::CONTROLLER_LISTEN),
    pnglistenconn(epoll, EPOLLIN, pngSock, RegionConnection::PNGVIEWER_LISTEN);
  
  vector<RegionConnection*> regions, controllers, pngviewers;
  size_t maxevents = 1 + server_count;
  struct epoll_event *events = new struct epoll_event[maxevents];
  size_t connected = 0, ready = 0;
  RegionInfo regioninfo;
  TimestepDone tsdone;
  TimestepUpdate timestep;
  ClaimTeam claimteam;
  unsigned long long step = 0;
  timestep.set_timestep(step++);
  time_t lastSecond = time(NULL);
  int timeSteps = 0;
  unsigned regionId = 0;
  int numConnectedServers = 0;
  
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
    	RegionConnection *c = (RegionConnection*)events[i].data.ptr;
      if(events[i].events & EPOLLIN) {
        switch(c->type) {
        case RegionConnection::REGION:
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
              region->set_id(numConnectedServers++);

              for(vector<RegionConnection*>::iterator i = controllers.begin();
                  i != controllers.end(); ++i) {
                (*i)->queue.push(MSG_REGIONINFO, *region);
                (*i)->set_writing(true);
              }

              for(vector<RegionConnection*>::iterator i = pngviewers.begin();
                  i != pngviewers.end(); ++i) {
                (*i)->queue.push(MSG_REGIONINFO, *region);
                (*i)->set_writing(true);            
              }
              
              break;
            }
            
            default:
              cerr << "Unexpected message from region!" << endl;
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
            for(vector<RegionConnection*>::iterator i = regions.begin();
                i != regions.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
              (*i)->set_writing(true);
            }
            // Send to controllers
            for(vector<RegionConnection*>::iterator i = controllers.begin();
                i != controllers.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
              (*i)->set_writing(true);
            }
          }
          break;
        }

        case RegionConnection::CONTROLLER:
        {
          MessageType type;
          size_t len;
          const void *buffer;
          try {
            if(c->reader.doRead(&type, &len, &buffer)) {
              switch(type) {
              case MSG_CLAIMTEAM:
              {
                claimteam.ParseFromArray(buffer, len);
                unsigned id = claimteam.id();
                if(teamclaimed[id]) {
                  cout << "Team " << id << " was already claimed!" << endl;
                  claimteam.set_granted(false);
                } else {
                  cout << "Team " << id << " has been claimed." << endl;
                  claimteam.set_granted(true);
                  teamclaimed[id] = true;
                }
                c->queue.push(MSG_CLAIMTEAM, claimteam);
                break;
              }
            
              default:
                cerr << "Unexpected message from controller!" << endl;
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
          break;
        }

        case RegionConnection::REGION_LISTEN:
        {
          // Accept a new region server
          struct sockaddr_in addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept region");
          }
          net::set_blocking(fd, false);

          RegionConnection *newconn = new RegionConnection(epoll, EPOLLIN, fd, RegionConnection::REGION);
          newconn->addr = addr.sin_addr.s_addr;
          regions.push_back(newconn);

          cout << "Got region server connection." << endl;

          ++connected;
          break;
        }

        case RegionConnection::CONTROLLER_LISTEN:
        {
          // Accept a new controller
          struct sockaddr_in addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept controller");
          }
          net::set_blocking(fd, false);

          RegionConnection *newconn = new RegionConnection(epoll, EPOLLIN | EPOLLOUT, fd, RegionConnection::CONTROLLER);
          newconn->addr = addr.sin_addr.s_addr;
          controllers.push_back(newconn);
          
          newconn->queue.push(MSG_WORLDINFO, worldinfo);

          cout << "Got controller connection." << endl;
          break;
        }
        case RegionConnection::PNGVIEWER_LISTEN:
        {
          // Accept a new pngviewer
          struct sockaddr_in addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept png viewer");
          }
          net::set_blocking(fd, false);

          RegionConnection *newconn = new RegionConnection(epoll, EPOLLOUT, fd, RegionConnection::PNGVIEWER);
          newconn->addr = addr.sin_addr.s_addr;
          pngviewers.push_back(newconn);

          for(size_t i = 0; i < (unsigned)worldinfo.region_size(); ++i) {
            newconn->queue.push(MSG_REGIONINFO, worldinfo.region(i));
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
        case RegionConnection::PNGVIEWER:
        case RegionConnection::CONTROLLER:
        case RegionConnection::REGION:
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
  close(epoll);
  for(vector<RegionConnection*>::iterator i = controllers.begin();
      i != controllers.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  for(vector<RegionConnection*>::iterator i = regions.begin();
      i != regions.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  close(sock);
  close(controllerSock);

delete[] teamclaimed;

  return 0;
}
