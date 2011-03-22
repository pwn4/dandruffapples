#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <iomanip>
#include <vector>
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

#include "../common/globalconstants.h"
#include "../common/except.h"
#include "../common/ports.h"
#include "../common/net.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/parseconf.h"

#include "../common/helper.h"
#include "../common/globalconstants.h"

using namespace std;

struct RegionConnection: public net::EpollConnection {
	in_addr_t addr;

	RegionConnection(int epoll, int flags, int fd, Type type) :
		net::EpollConnection(epoll, flags, fd, type) {
	}
};

//define variables
const char *configFileName;

unsigned server_count = 1;
int server_rows = 1;
int server_cols = 1;

WorldInfo worldinfo;

//this function takes a draw_x that you give it, and wraps it around servercols
unsigned int wrapX(int x)
{
  if(x < 0)
    return x + server_cols;
  
  if(x >= server_cols)
    return x - server_cols;
    
  return x;
}
//this function takes a draw_y that you give it, and wraps it around serverrows
unsigned int wrapY(int y)
{
  if(y < 0)
    return y + server_rows;
  
  if(y >= server_rows)
    return y - server_rows;
    
  return y;
}


struct xyCoord {
	int x;
	int y;
};
//these variables need to be global
size_t ready = 0;
vector<RegionConnection*> regions, controllers, worldviewers;
size_t connected = 0, worldinfoSent = 0;
RegionInfo regioninfo;
TimestepDone tsdone;
TimestepUpdate timestep;
ClaimTeam claimteam;
unsigned long long step = 0;
int numPositionedServers = 0;
bool running = false;

void checkNewStep() {
  static time_t lastSecond = time(NULL);
  static unsigned long long timeSteps = 0;
  const size_t interval = 20;      // seconds
  static unsigned pastStepCounts[10];
  static unsigned long long seconds = 0;
  if (ready == server_count + controllers.size() && running) {
    ++timeSteps;
    time_t now = time(NULL);
		// Check if a second has passed
		if(now > lastSecond) {
      lastSecond = now;
      ++seconds;

      cout << setprecision(1) << fixed;
      cout << timeSteps / 2.0f << " ts/s | ";

      // Update record and sum total
      unsigned total = 0;
      for(unsigned i = interval - 1; i > 0; --i) {
        pastStepCounts[i] = pastStepCounts[i-1];
        total += pastStepCounts[i];
      }
      pastStepCounts[0] = timeSteps;
      total += timeSteps;
      timeSteps = 0;

      if(seconds > (interval + 1)) {
        // We have enough data to do stats
        float mean = ((float)total / (float)interval) / 2.0f;
      
        cout << mean << " avg | ";
      
        // Calculate standard deviation
        float sumOfSquares = 0;
        for(unsigned i = 0; i < interval; ++i) {
          float delta = (pastStepCounts[i]/2.0f) - mean;
          sumOfSquares += delta * delta;
        }
        float stddev = sqrt(sumOfSquares/(float)(interval-1));
        cout << stddev << " std dev";
      } else {
        cout << " gathering data...";
      }
      cout << endl;
		}

		// All servers are ready, prepare to send next step
		ready = 0;
		timestep.set_timestep(step++);
		// Send to regions
		for (vector<RegionConnection*>::const_iterator i = regions.begin(); i != regions.end(); ++i) {
			(*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
			(*i)->set_writing(true);
		}

		// Send to controllers -- EVERY SINGLE TIMESTEP
		for (vector<RegionConnection*>::const_iterator i = controllers.begin(); i != controllers.end(); ++i) {
			(*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
			(*i)->set_writing(true);
		}
	}

}

//handle the drawing of home
//going to draw the homes to have the minimal impact on the performance of the system
void handleHomes(int teams, int serverCount) {
	srand ( time(NULL) );
	//int regionid, regionHomeNum, draw co-ordinates
	map<int, map<int, xyCoord> > home;

	//draw homes uniformly,
	int homesPerRegion = teams / serverCount;
	int leftOverHomes = teams % serverCount;
	int coord[2];

	//loop through all the regions
	for (int i = 0; i < serverCount; i++) {
		//get homesPerRegion homes per region
		for (int j = 0; j < homesPerRegion; j++) {
			//don't draw the home too close to the border of the region
			coord[0] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);
			coord[1] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);

			//make sure that the new home is at least MINDISTANCEFROMHOME units away from all the other homes in the area
			for (int k = 0; k < j; k++) {
				if (helper::distanceBetween((float)home[i][k].x, (float)coord[0], (float)home[i][k].y, (float)coord[1]) <= MINDISTANCEFROMHOME) {
#ifdef DEBUG

					cerr << "Failed because distance is " + helper::toString(
							helper::distanceBetween(home[i][k].x, coord[0], home[i][k].y, coord[1])) << endl;
					cerr << "Failed with (" + helper::toString(home[i][k].x) + ", " + helper::toString(home[i][k].y) + "), ("+ helper::toString(coord[0])  + ", "+ helper::toString(coord[1])+")"<<endl;
#endif
					//get new coordinates
					coord[0] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);
					
					coord[1] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);
#ifdef DEBUG
					cerr << "New house at (" + helper::toString(coord[0]) + ", " + helper::toString(coord[1]) + ")"
							<< endl << endl;
#endif

					//restart the loop
					k = -1;
					continue;
				}
			}

			home[i][j].x = coord[0];
			home[i][j].y = coord[1];

#ifdef DEBUG
			cout<<"             New home on region " + helper::toString(i) + " for team " + helper::toString(i * homesPerRegion + j) + " at ("+ helper::toString(coord[0]) + ", " + helper::toString(coord[1]) + ")"<<endl;

#endif
			HomeInfo *homeInfo=worldinfo.add_home();
			homeInfo->set_region_id(i);
			homeInfo->set_home_x(coord[0]);
			homeInfo->set_home_y(coord[1]);
			homeInfo->set_team(i * homesPerRegion + j);
		}
	}
#ifdef DEBUG
	cout << "/////////////////////////////////////////////////////////////////////////" << endl;
	cout << "/////////////////////////////////////////////////////////////////////////" << endl;
	cout << "/////////////////////////////////////////////////////////////////////////" << endl;
	cout << "/////////////////////////////////////////////////////////////////////////" << endl;
#endif
	//handle the odd homes at one home per server
	//loop through all the regions
	for (int i = 0; i < leftOverHomes; i++) {
		coord[0] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);
		coord[1] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);

		//make sure that the new home is at least MINDISTANCEFROMHOME units away from all the other homes in the area
		for (unsigned int j = 0; j < home[i].size(); j++) {
			if (helper::distanceBetween((float)home[i][j].x, (float)coord[0], (float)home[i][j].y, (float)coord[1]) <= MINDISTANCEFROMHOME) {
#ifdef DEBUG
				cerr << "Failed because distance is " + helper::toString(
						helper::distanceBetween(home[i][j].x, coord[0], home[i][j].y, coord[1])) << endl;
				cerr << "Failed with (" + helper::toString(home[i][j].x) + ", " + helper::toString(home[i][j].y) + "), ("+ helper::toString(coord[0])  + ", "+ helper::toString(coord[1])+")"<<endl;
#endif
				//get new coordinates
				coord[0] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);
				coord[1] = MINDISTANCEFROMHOME / 2 + rand() % (REGIONSIDELEN - MINDISTANCEFROMHOME);

#ifdef DEBUG
				cerr << "New house at (" + helper::toString(coord[0]) + ", " + helper::toString(coord[1]) + ")" << endl
						<< endl;
#endif
				//restart the loop
				j = -1;
				continue;
			}
		}

		home[i][home[i].size()].x = coord[0];
		home[i][home[i].size()-1].y = coord[1];

#ifdef DEBUG
		cout << "             New home on region " + helper::toString(i) + " for team " + helper::toString(i + home.size() * (home[i].size()-1)) + " at ("
				+ helper::toString(coord[0]) + ", " + helper::toString(coord[1]) + ")" << endl;
#endif

		HomeInfo *homeInfo=worldinfo.add_home();
		homeInfo->set_region_id(i);
		homeInfo->set_home_x(coord[0]);
		homeInfo->set_home_y(coord[1]);
		homeInfo->set_team(i + home.size() * (home[i].size()-1));
	}
}

int main(int argc, char **argv) {
	helper::CmdLine cmdline(argc, argv);
	configFileName = cmdline.getArg("-c", "config").c_str();
	cout << "Using config file: " << configFileName << endl;

	//loadConfigFile();
	conf configuration = parseconf(configFileName);
	if (configuration.find("SERVERROWS") == configuration.end() || configuration.find("SERVERCOLS")
			== configuration.end() || configuration.find("TEAMS") == configuration.end() || configuration.find(
			"ROBOTS_PER_TEAM") == configuration.end()) {
		cerr << "Config file is missing an entry!" << endl;
		return 1;
	}

	//drawing setup
	server_rows = strtol(configuration["SERVERROWS"].c_str(), NULL, 10);
	server_cols = strtol(configuration["SERVERCOLS"].c_str(), NULL, 10);
	server_count = server_rows * server_cols;
	int ** drawGrid = new int*[server_cols];
	for (int i = 0; i < server_cols; i++)
		drawGrid[i] = new int[server_rows];
	int freeX = 0, freeY = 0;

	bool *teamclaimed;
	// Create initial world state
	worldinfo.set_numpucks(1000);//send the number of pucks in the region
	unsigned teams = atoi(configuration["TEAMS"].c_str());
	unsigned robots_per_team = atoi(configuration["ROBOTS_PER_TEAM"].c_str());
  unsigned teamsLeft = teams;

	handleHomes(teams, server_count);

	// Note that robot ID 0 is invalid.
	unsigned id = 1;
	teamclaimed = new bool[teams];
	for (unsigned team = 0; team < teams; ++team)
	  teamclaimed[team] = false;
	/*for (unsigned team = 0; team < teams; ++team) {
		teamclaimed[team] = false;
		for (unsigned robot = 0; robot < robots_per_team; ++robot) {
			RobotInfo *i = worldinfo.add_robot();
			i->set_id(id);
			i->set_region(region);
			i->set_team(team);
			++id;
			region = (region + 1) % server_count;
		}
	}*/
	///////////////new robot generating loop
	int wantRobots = teams * robots_per_team;
	int numRobots = 0;

	for (unsigned int j = (2 * ROBOTDIAMETER)+(4 * MINELEMENTSIZE); j < (REGIONSIDELEN+(1 * MINELEMENTSIZE)) - (2 * (ROBOTDIAMETER)+(4 * MINELEMENTSIZE)) && numRobots	< wantRobots; j += 5 * (ROBOTDIAMETER)) {
	  for (unsigned int i = (2 * ROBOTDIAMETER)+(4 * MINELEMENTSIZE); i < (REGIONSIDELEN+(1 * MINELEMENTSIZE)) - (2 * (ROBOTDIAMETER)+(4 * MINELEMENTSIZE)) && numRobots	< wantRobots; i += 5 * (ROBOTDIAMETER)){

      //we repeat this across all regions
      for(unsigned int k = 0; k < server_count && numRobots < wantRobots; k++){

        RobotInfo *ri = worldinfo.add_robot();
			  ri->set_id(id);
			  ri->set_region(k);
			  ri->set_team((id-1)/robots_per_team);

			  ri->set_x(i);
			  ri->set_y(j); 
			  ++id;
		    numRobots++;
		  }
	  }
  }
	///////////////

	// Disregard SIGPIPE so we can handle things normally
	signal(SIGPIPE, SIG_IGN);

	int sock = net::do_listen(CLOCK_PORT);
	int controllerSock = net::do_listen(CONTROLLERS_PORT);
	int worldviewSock = net::do_listen(WORLD_VIEWER_PORT);
	net::set_blocking(sock, false);
	net::set_blocking(controllerSock, false);
	net::set_blocking(worldviewSock, false);

	int epoll = epoll_create(server_count);
	if (epoll < 0) {
		perror("Failed to create epoll handle");
		close(sock);
		close(controllerSock);
		close(worldviewSock);
		return 1;
	}

	RegionConnection listenconn(epoll, EPOLLIN, sock, RegionConnection::REGION_LISTEN),
	controllerlistenconn(epoll,	EPOLLIN, controllerSock, RegionConnection::CONTROLLER_LISTEN),
	worldlistenconn(epoll, EPOLLIN,	worldviewSock, RegionConnection::WORLDVIEWER_LISTEN);

	net::EpollConnection standardinput(epoll, 0, STDIN_FILENO, net::connection::STDIN);

	size_t maxevents = 1 + server_count;
	struct epoll_event *events = new struct epoll_event[maxevents];
	timestep.set_timestep(step++);

	cout << "Listening for connections." << endl;
	while (true) {
		int eventcount;
		do {
			eventcount = epoll_wait(epoll, events, maxevents, -1);
		} while (eventcount < 0 && errno == EINTR);
		if (eventcount < 0) {
			perror("Failed to wait on sockets");
			break;
		}

		for (size_t i = 0; i < (unsigned) eventcount; ++i) {
			RegionConnection *c = (RegionConnection*) events[i].data.ptr;
			if (events[i].events & EPOLLIN) {
				switch (c->type) {
				case RegionConnection::REGION: {
					MessageType type;
					int len;
					const void *buffer;
					try {
						if (c->reader.doRead(&type, &len, &buffer)) {
							switch (type) {
							case MSG_TIMESTEPDONE: {
								tsdone.ParseFromArray(buffer, len);
								++ready;

								checkNewStep();

								break;
							}
							case MSG_REGIONINFO: {
								if (worldinfoSent == server_count) {
									cerr << "Unexpected RegionInfo message!" << endl;
									break;
								}

								RegionInfo *region = worldinfo.add_region();
								region->ParseFromArray(buffer, len);
								region->set_address(c->addr);
								region->set_id(numPositionedServers);

								//calculate where to draw it
								region->set_draw_x(freeX++);
								region->set_draw_y(freeY);
								numPositionedServers++;

								if (freeX == server_cols) {
									freeX = 0;
									freeY++;
								}

								// New server has no neighbours initially.
								for (int i = 0; i < numPositionedServers - 1; i++) {
									worldinfo.mutable_region(i)->clear_position();
								}

								// Go through all the connected servers' RegionInfo messages,
								// and set Position if it is a neighbour of the new server.
								// For example, if 1 is above 4, then we set Position of
								// 1 to be TOP.
								if (numPositionedServers > 1) {
								  
								  //iterate through the world object, and add known neighbours that way.
								  for(int i = 0; i < worldinfo.region_size(); i++)
								  {
								    //don't check ourselves
								    if(worldinfo.region(i).id() == region->id())
								      continue;
								    
								    RegionInfo otherRegion = worldinfo.region(i);
								    //worldinfo.mutable_region(serverid)->add_position(RegionInfo_Position_TOP_LEFT);
								    if(wrapX(region->draw_x() - 1) == otherRegion.draw_x() && wrapY(region->draw_y() - 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_TOP_LEFT);
								      
								    if(region->draw_x() == otherRegion.draw_x() && wrapY(region->draw_y() - 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_TOP);

								    if(wrapX(region->draw_x() + 1) == otherRegion.draw_x() && wrapY(region->draw_y() - 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_TOP_RIGHT);

								    if(wrapX(region->draw_x() - 1) == otherRegion.draw_x() && region->draw_y() == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_LEFT);

								    if(wrapX(region->draw_x() + 1) == otherRegion.draw_x() && region->draw_y() == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_RIGHT);
								    
								    if(wrapX(region->draw_x() - 1) == otherRegion.draw_x() && wrapY(region->draw_y() + 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_BOTTOM_LEFT);
								    
								    if(region->draw_x() == otherRegion.draw_x() && wrapY(region->draw_y() + 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_BOTTOM);
								    
								    if(wrapX(region->draw_x() + 1) == otherRegion.draw_x() && wrapY(region->draw_y() + 1) == otherRegion.draw_y())
								      worldinfo.mutable_region(otherRegion.id())->add_position(RegionInfo_Position_BOTTOM_RIGHT);
								    
								  }
								  
								}

								// Send WorldInfo to the new RegionServer
								c->queue.push(MSG_WORLDINFO, worldinfo);
								c->set_writing(true);
								worldinfoSent++;

								for (vector<RegionConnection*>::const_iterator i = controllers.begin(); i
										!= controllers.end(); ++i) {
									(*i)->queue.push(MSG_REGIONINFO, *region);
									(*i)->set_writing(true);
								}

								for (vector<RegionConnection*>::const_iterator i = worldviewers.begin(); i
										!= worldviewers.end(); ++i) {
									(*i)->queue.push(MSG_REGIONINFO, *region);
									(*i)->queue.push(MSG_WORLDINFO, worldinfo);
									(*i)->set_writing(true);
								}

								// Initialization: If all servers connected, and we sent out
								// WorldInfo packets to all, then send the first timestep out!
								if (connected == server_count && worldinfoSent == server_count) {
									cout << "All region servers connected!  Press return to begin simulation: "
											<< flush;
									standardinput.set_reading(true);
								}

								break;
							}

							default:
								cerr << "Unexpected message from region!" << endl;
								break;
							}
						}
					} catch (EOFError e) {
						//and a region server has disconnected
						if ((ready == connected && connected == server_count) || running) {
							cerr << "Region server disconnected!  Shutting down." << endl;
							return 1;
						} else {
							close(c->fd);

							regions.erase(find(regions.begin(), regions.end(), c));
							connected--;
							delete c;
							cerr << "Region server disconnected!" << endl;

							break;
						}
					} catch (SystemError e) {
						cerr << "Error reading from region server: " << e.what() << ".  Shutting down." << endl;
						return 1;
					}
					// DON'T PUT ANY CODE HERE, IT'S THE WRONG PLACE.
					break;
				}

				case RegionConnection::CONTROLLER: {
					MessageType type;
					int len;
					const void *buffer;
					try {
						if (c->reader.doRead(&type, &len, &buffer)) {
							switch (type) {
							case MSG_CLAIMTEAM: {
								claimteam.ParseFromArray(buffer, len);
								unsigned id = claimteam.id();
								if (teamclaimed[id]) {
									cout << "Team " << id << " was already claimed!" << endl;
									claimteam.set_granted(false);
								} else {
                  --teamsLeft;
									cout << "Team " << id << " has been claimed, "
                       << teamsLeft << " remaining." << endl;
									claimteam.set_granted(true);
									teamclaimed[id] = true;

									//find the team's home
									const HomeInfo * teamsHome = NULL;
									for(int i = 0; i < worldinfo.home_size(); i++)
									  if(worldinfo.home(i).team() == id)
									  {
									    teamsHome = &(worldinfo.home(i));
									    break;
								    }
								  //find the home's region
									const RegionInfo * homesRegion = NULL;
									for(int i = 0; i < worldinfo.region_size(); i++)
									  if(worldinfo.region(i).id() == teamsHome->region_id())
									  {
									    homesRegion = &(worldinfo.region(i));
									    break;
								    }

									//now, send the home info for the robots
									for(int i = 0; i < worldinfo.robot_size(); i++)
									{
									  if(worldinfo.robot(i).team() == id)
									  {
							  			RobotHomeInfo *rhi = claimteam.add_homes();
							  			RegionInfo robotRegion;  //we need the robot's region as well
							  			for(int j = 0; j < worldinfo.region_size(); j++)
							  			  if(worldinfo.region(j).id() == worldinfo.robot(i).region())
							  			  {
							  			    robotRegion = worldinfo.region(j);
							  			    break;
						  			    }
							  			rhi->set_id(worldinfo.robot(i).id());
							  			rhi->set_relx((float) ( (homesRegion->draw_x() * REGIONSIDELEN) + teamsHome->home_x() ) - ((robotRegion.draw_x() * REGIONSIDELEN) + worldinfo.robot(i).x()));
							  			rhi->set_rely((float) ( (homesRegion->draw_y() * REGIONSIDELEN) + teamsHome->home_y() ) - ((robotRegion.draw_y() * REGIONSIDELEN) + worldinfo.robot(i).y()));
									  }
									}
								}
                if(teamsLeft == 0) {
                  cout << "All teams claimed!" << endl;
                }
								c->queue.push(MSG_CLAIMTEAM, claimteam);
								c->set_writing(true);
								break;
							}
							case MSG_TIMESTEPDONE: {
								tsdone.ParseFromArray(buffer, len);
								++ready;

								checkNewStep();

								break;
							}

							default:
								cerr << "Unexpected message from controller!" << endl;
								break;
							}

						}
					} catch (EOFError e) {
						cerr << "Controller disconnected!  Shutting down." << endl;
						return 1;
					} catch (SystemError e) {
						cerr << "Error reading from controller: " << e.what() << ".  Shutting down." << endl;
						return 1;
					}
					break;
				}

				case RegionConnection::REGION_LISTEN: {
					if (connected == server_count) {
						cerr << "Unexpected region connection!" << endl;
						break;
					}

					// Accept a new region server
					struct sockaddr_in addr;
					socklen_t addr_size = sizeof(addr);
					int fd = accept(c->fd, (struct sockaddr*) &addr, &addr_size);
					if (fd < 0) {
						throw SystemError("Failed to accept region");
					}
					net::set_blocking(fd, false);

					RegionConnection *newconn = new RegionConnection(epoll, EPOLLIN, fd, RegionConnection::REGION);
					newconn->addr = addr.sin_addr.s_addr;
					regions.push_back(newconn);

					++connected;
					cout << "Region server " << connected << "/" << server_count << " connected." << endl;

					if (connected == server_count) {
						// Look for no further connections.
						listenconn.set_reading(false);
					}

					break;
				}

				case net::connection::STDIN:
				  //we need to only send the first timestep when all regions are connected to each other
					ready = 0;
					timestep.set_timestep(0);
				
					cout << "Running!" << endl;
					// We don't care about stdin anymore.
					standardinput.set_reading(false);
					// Step continuously
					running = true;
					
					// Send to regions
					for (vector<RegionConnection*>::const_iterator i = regions.begin(); i != regions.end(); ++i) {
						(*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
						(*i)->set_writing(true);
					}
					// Send to controllers
					for (vector<RegionConnection*>::const_iterator i = controllers.begin(); i != controllers.end(); ++i) {
						(*i)->queue.push(MSG_TIMESTEPUPDATE, timestep);
						(*i)->set_writing(true);
					}
					break;

				case RegionConnection::CONTROLLER_LISTEN: {
					// Accept a new controller
					struct sockaddr_in addr;
					socklen_t addr_size = sizeof(addr);
					int fd = accept(c->fd, (struct sockaddr*) &addr, &addr_size);
					if (fd < 0) {
						throw SystemError("Failed to accept controller");
					}
					net::set_blocking(fd, false);

					RegionConnection *newconn = new RegionConnection(epoll, EPOLLIN | EPOLLOUT, fd,
							RegionConnection::CONTROLLER);
					newconn->addr = addr.sin_addr.s_addr;
					controllers.push_back(newconn);

					newconn->queue.push(MSG_WORLDINFO, worldinfo);
					newconn->set_writing(true);

					cout << "Got controller connection." << endl;
					break;
				}
				case RegionConnection::WORLDVIEWER_LISTEN: {
					// Accept a new worldviewer
					struct sockaddr_in addr;
					socklen_t addr_size = sizeof(addr);
					int fd = accept(c->fd, (struct sockaddr*) &addr, &addr_size);
					if (fd < 0) {
						throw SystemError("Failed to accept world viewer");
					}
					net::set_blocking(fd, false);

					RegionConnection *newconn =
							new RegionConnection(epoll, EPOLLOUT, fd, RegionConnection::WORLDVIEWER);
					newconn->addr = addr.sin_addr.s_addr;
					worldviewers.push_back(newconn);

					for (size_t i = 0; i < (unsigned) worldinfo.region_size(); ++i) {
						newconn->queue.push(MSG_REGIONINFO, worldinfo.region(i));
					}

					newconn->queue.push(MSG_WORLDINFO, worldinfo);
					newconn->set_writing(true);

					cout << "Got world viewer connection." << endl;
					break;
				}

				default:
					cerr << "Internal error: Got unexpected readable event!" << endl;
					break;
				}
			} else if (events[i].events & EPOLLOUT) {
				switch (c->type) {
				case RegionConnection::WORLDVIEWER:
				case RegionConnection::CONTROLLER:
				case RegionConnection::REGION:
					if (c->queue.doWrite()) {
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
	for (vector<RegionConnection*>::const_iterator i = controllers.begin(); i != controllers.end(); ++i) {
		shutdown((*i)->fd, SHUT_RDWR);
		close((*i)->fd);
    delete *i;
	}
	for (vector<RegionConnection*>::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		shutdown((*i)->fd, SHUT_RDWR);
		close((*i)->fd);
    delete *i;
	}
	close(sock);
	close(controllerSock);

	delete[] teamclaimed;

	return 0;
}
