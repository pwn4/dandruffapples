#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <vector>
#include <math.h>
#include <algorithm>

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

WorldInfo worldinfo;

void addPositions(int newServerRow, int newServerCol, int numRows, 
                  int numCols, int numColsInLastRow) {
  int thisRow;
  int thisCol;
  int serverid;

  // TOP_LEFT:
  thisRow = (newServerRow - 1 + numRows) % numRows;
  thisCol = (newServerCol - 1 + numCols) % numCols;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_TOP_LEFT);
  
  // TOP:
  thisRow = (newServerRow - 1 + numRows) % numRows;
  thisCol = newServerCol;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_TOP);
  
  // TOP_RIGHT:
  thisRow = (newServerRow - 1 + numRows) % numRows;
  thisCol = (newServerCol + 1) % numCols;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_TOP_RIGHT);
  
  // RIGHT:
  thisRow = newServerRow;
  thisCol = (newServerCol + 1) % numColsInLastRow;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_RIGHT);
  
  // BOTTOM_RIGHT:
  thisRow = 0;
  thisCol = (newServerCol + 1) % numCols;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_BOTTOM_RIGHT);
  
  // BOTTOM:
  thisRow = 0; 
  thisCol = newServerCol;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_BOTTOM);
  
  // BOTTOM_LEFT:
  thisRow = 0;
  thisCol = (newServerCol - 1 + numCols) % numCols;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_BOTTOM_LEFT);
  
  // LEFT:
  thisRow = newServerRow; 
  thisCol = (newServerCol - 1 + numColsInLastRow) % numColsInLastRow;
  serverid = ((thisRow * server_cols) + thisCol);
  worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_LEFT);
}

int main(int argc, char **argv) {
	helper::CmdLine cmdline(argc, argv);
	configFileName=cmdline.getArg("-c", "config").c_str();
	cout<<"Using config file: "<<configFileName<<endl;

  //loadConfigFile();
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
  int freeX = 0, freeY = 0;

  bool *teamclaimed;
  {                             // Create initial world state
		worldinfo.set_numpucks(1000);//send the number of pucks in the region
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
  int worldviewSock = net::do_listen(WORLD_VIEWER_PORT);
  net::set_blocking(sock, false);
  net::set_blocking(controllerSock, false);
  net::set_blocking(worldviewSock, false);

  int epoll = epoll_create(server_count);
  if(epoll < 0) {
    perror("Failed to create epoll handle");
    close(sock);
    close(controllerSock);
    close(worldviewSock);
    return 1;
  }

  RegionConnection listenconn(epoll, EPOLLIN, sock, RegionConnection::REGION_LISTEN),
    controllerlistenconn(epoll, EPOLLIN, controllerSock, RegionConnection::CONTROLLER_LISTEN),
    worldlistenconn(epoll, EPOLLIN, worldviewSock, RegionConnection::WORLDVIEWER_LISTEN);
  net::EpollConnection standardinput(epoll, 0, STDIN_FILENO, net::connection::STDIN);
  
  vector<RegionConnection*> regions, controllers, worldviewers;
  size_t maxevents = 1 + server_count;
  struct epoll_event *events = new struct epoll_event[maxevents];
  size_t connected = 0, ready = 0, worldinfoSent = 0;
  bool initialized = false;
  RegionInfo regioninfo;
  TimestepDone tsdone;
  TimestepUpdate timestep;
  ClaimTeam claimteam;
  unsigned long long step = 0;
  timestep.set_timestep(step++);
  time_t lastSecond = time(NULL);
  int timeSteps = 0;
  int numPositionedServers = 0;
  
  long totalpersecond = 0, number = 0;
  long values[1000];
  long freeval = 0;
  int second = 0;
  bool running = false;

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
          int len;
          const void *buffer;
          try {
            if(c->reader.doRead(&type, &len, &buffer)) {
            switch(type) {
            case MSG_TIMESTEPDONE:
            {
              tsdone.ParseFromArray(buffer, len);
              ++ready;

              if(ready == server_count && running) {
                //check if its time to output
                if(time(NULL) > lastSecond)
                {
                  cout << timeSteps/2 << " timesteps/second. second: " << second++ << endl;
                  //do some stats calculations
                  totalpersecond += timeSteps/2;
                  number++;
                  values[freeval++] = (long)timeSteps/2;
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
                for(vector<RegionConnection*>::const_iterator i = regions.begin();
                    i != regions.end(); ++i) {
                  (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
                  (*i)->set_writing(true);
                }
                // Send to controllers
                for(vector<RegionConnection*>::const_iterator i = controllers.begin();
                    i != controllers.end(); ++i) {
                  (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
                  (*i)->set_writing(true);
                }
              }
              break;
            }
            case MSG_REGIONINFO:
            {
              RegionInfo *region = worldinfo.add_region();
              region->ParseFromArray(buffer, len);
              region->set_address(c->addr);
              region->set_id(numPositionedServers);

              //calculate where to draw it
              region->set_draw_x(freeX++);
              region->set_draw_y(freeY);
              numPositionedServers++;

              if(freeX == server_cols)
              {
                freeX = 0;
                freeY++;
              }

              // Assume servers populate from left to right, then from top 
              // to bottom.
              int numRows = ((numPositionedServers - 1) / server_cols) + 1;
              int numCols = numPositionedServers < server_cols ? 
                  numPositionedServers : server_cols;
              int numColsInLastRow = ((numPositionedServers - 1) % server_cols) + 1;
              int newServerRow = (numPositionedServers - 1) / server_cols;
              int newServerCol = (numPositionedServers - 1) % server_cols;

              // New server has no neighbours initially.
              for(int i = 0; i < numPositionedServers - 1; i++) { 
                worldinfo.mutable_region(i)->clear_position();
              }

              // Go through all the connected servers' RegionInfo messages, 
              // and set Position if it is a neighbour of the new server.
              // For example, if 1 is above 4, then we set Position of
              // 1 to be TOP. 
              if (numPositionedServers > 1) {
                addPositions(newServerRow, newServerCol, numRows, 
                    numCols, numColsInLastRow);
              }

              // Send WorldInfo to the new RegionServer
              c->queue.push(MSG_WORLDINFO, worldinfo);
              c->set_writing(true);
              worldinfoSent++;

              for(vector<RegionConnection*>::const_iterator i = controllers.begin();
                  i != controllers.end(); ++i) {
                (*i)->queue.push(MSG_REGIONINFO, *region);
                (*i)->set_writing(true);
              }

              for(vector<RegionConnection*>::const_iterator i = worldviewers.begin();
                  i != worldviewers.end(); ++i) {
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
        	  //added the last "seconds>0" check to test whether we started running the simulation
        	  //and a region server has disconnected
        	  if( ( ready == connected && connected == server_count ) ||  second > 0)
        	  {
        		  cerr << "Region server disconnected!  Shutting down." << endl;
            	return 1;
        	  }
        	  else
        	  {
        		  close(c->fd);

        		  regions.erase(find(regions.begin(), regions.end(), c));
        		  connected--;
        		  delete c;
        		  cerr << "Region server disconnected!"<<endl;

        		  break;
        	  }
          } catch(SystemError e) {
            cerr << "Error reading from region server: "
                 << e.what() << ".  Shutting down." << endl;
            return 1;
          }

          // Initialization: If all servers connected, and we sent out
          // WorldInfo packets to all, then send the first timestep out!
          if(connected == server_count && worldinfoSent == server_count &&
             !initialized) {
            initialized = true; 
            listenconn.set_reading(false);
            // Send initialization timestep.
            ready = 0;
            timestep.set_timestep(0);
            for(vector<RegionConnection*>::const_iterator i = regions.begin();
                i != regions.end(); ++i) {
              (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
              (*i)->set_writing(true);
            }
            cout << "All region servers connected!  Press return to begin simulation: " << flush;
            standardinput.set_reading(true);
          }

          break;
        }

        case RegionConnection::CONTROLLER:
        {
          MessageType type;
          int len;
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
            cerr << "Controller disconnected!  Shutting down." << endl;
            return 1;
          } catch(SystemError e) {
            cerr << "Error reading from controller: "
                 << e.what() << ".  Shutting down." << endl;
            return 1;
          }
          break;
        }

        case RegionConnection::REGION_LISTEN:
        {
          //if we have all the region servers we need, 'ignore' any more
          if(connected == server_count)
            break;
        
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

          ++connected;
          cout << "Region server " << connected << "/" << server_count << " connected." << endl;

          break;
        }

        case net::connection::STDIN:
          cout << "Running!" << endl;
          // We don't care about stdin anymore.
          standardinput.set_reading(false);
          // Step continuously
          running = true;
          // Send first timestep
          ready = 0;
          timestep.set_timestep(step++);
          // Send to regions
          for(vector<RegionConnection*>::const_iterator i = regions.begin();
              i != regions.end(); ++i) {
            (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
            (*i)->set_writing(true);
          }
          // Send to controllers
          for(vector<RegionConnection*>::const_iterator i = controllers.begin();
              i != controllers.end(); ++i) {
            (*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
            (*i)->set_writing(true);
          }          
          break;

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
        case RegionConnection::WORLDVIEWER_LISTEN:
        {
          // Accept a new worldviewer
          struct sockaddr_in addr;
          socklen_t addr_size = sizeof(addr);
          int fd = accept(c->fd, (struct sockaddr*)&addr, &addr_size);
          if(fd < 0) {
            throw SystemError("Failed to accept world viewer");
          }
          net::set_blocking(fd, false);

          RegionConnection *newconn = new RegionConnection(epoll, EPOLLOUT, fd, RegionConnection::WORLDVIEWER);
          newconn->addr = addr.sin_addr.s_addr;
          worldviewers.push_back(newconn);

          for(size_t i = 0; i < (unsigned)worldinfo.region_size(); ++i) {
            newconn->queue.push(MSG_REGIONINFO, worldinfo.region(i));
          }

          cout << "Got world viewer connection." << endl;
          break;
        }

        default:
          cerr << "Internal error: Got unexpected readable event!" << endl;
          break;
        }
      } else if(events[i].events & EPOLLOUT) {
        switch(c->type) {
        case RegionConnection::WORLDVIEWER:
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
  for(vector<RegionConnection*>::const_iterator i = controllers.begin();
      i != controllers.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  for(vector<RegionConnection*>::const_iterator i = regions.begin();
      i != regions.end(); ++i) {
    shutdown((*i)->fd, SHUT_RDWR);
    close((*i)->fd);
  }
  close(sock);
  close(controllerSock);

delete[] teamclaimed;

  return 0;
}
