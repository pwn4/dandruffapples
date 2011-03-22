/*/////////////////////////////////////////////////////////////////////////////////////////////////
 Regionserver program
 This program communications with clients, controllers, Worldviewers, other regionservers, and clockservers.
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <map>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <math.h>

#include <google/protobuf/message_lite.h>

#include "../common/claim.pb.h"
#include "../common/clientrobot.pb.h"
#include "../common/regionupdate.pb.h"
#include "../common/worldinfo.pb.h"
#include "../common/regionrender.pb.h"
#include "../common/timestep.pb.h"

#include "../common/ports.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/messagequeue.h"
#include "../common/net.h"
#include "../common/except.h"
#include "../common/parseconf.h"

#include "areaengine.h"

#include "../common/helper.h"

using namespace std;
/////////////////Variables and Declarations/////////////////
const char *configFileName;

//Config variables
char clockip[40] = "127.0.0.1";

int controllerPort = CONTROLLERS_PORT;
int worldviewerPort = WORLD_VIEWER_PORT;
int regionPort = REGIONS_PORT;
////////////////////////////////////////////////////////////

//need to tell whether a bool has been initialized or not
struct Bool {
	bool value;
	bool initialized;

	Bool() :
		initialized(false) {
	}
	;
};

void loadConfigFile() {
	//load the config file

	conf configuration = parseconf(configFileName);

	//clock ip address
	if (configuration.find("CLOCKIP") == configuration.end()) {
		cerr << "Config file is missing an entry!" << endl;
		exit(1);
	}
	strcpy(clockip, configuration["CLOCKIP"].c_str());

	//controller listening port
	if (configuration.find("CTRLPORT") != configuration.end()) {
		controllerPort = strtol(configuration["CTRLPORT"].c_str(), NULL, 10);
	}

	//region server listening port
	if (configuration.find("REGPORT") != configuration.end()) {
		regionPort = strtol(configuration["REGPORT"].c_str(), NULL, 10);
	}

	//world viewer listening port
	if (configuration.find("WORLDVIEWERPORT") != configuration.end()) {
		worldviewerPort = strtol(configuration["WORLDVIEWERPORT"].c_str(), NULL, 10);
	}

}

char *parse_port(char *input) {
	signed int input_len = (int) strlen(input);
	char *port;

	for (port = input; *port != ':' && (port - input) < input_len; ++port)
		;

	if ((port - input) == input_len) {
		return NULL;
	} else {
		// Split the string
		*port = '\0';
		++port;
		// Strip newline
		char *end;
		for (end = port; *end != '\n'; ++end)
			;
		if (end == port) {
			return NULL;
		} else {
			*end = '\0';
			return port;
		}
	}
}

//the main function
void run() {
	map<int, Bool> sendMoreWorldViews;
	struct timeval timeCache, microTimeCache;
	gettimeofday(&microTimeCache, NULL);
	bool generateImage;
	vector<HomeInfo*> myHomes;

	vector<pair <int, int> >uniqueRegions;
	vector<int> regionsAdded;

	//this is only here to generate random numbers for the logging
	srand(time(NULL));

	// Disregard SIGPIPE so we can handle things normally
	signal(SIGPIPE, SIG_IGN);

	//connect to the clock server
	cout << "Connecting to clock server..." << flush;
	int clockfd = net::do_connect(clockip, CLOCK_PORT);
	if (0 > clockfd) {
		perror(" failed to connect to clock server");
		exit(1);
		;
	} else if (0 == clockfd) {
		cerr << " invalid address: " << clockip << endl;
		exit(1);
		;
	}
	cout << " done." << endl;

	//listen for controller connections
	int controllerfd = net::do_listen(controllerPort);
	net::set_blocking(controllerfd, false);

	//listen for region server connections
	int regionfd = net::do_listen(regionPort);
	net::set_blocking(regionfd, false);

	//listen for world viewer connections
	int worldviewerfd = net::do_listen(worldviewerPort);
	net::set_blocking(worldviewerfd, false);

#ifdef ENABLE_LOGGING
	//create a new file for logging
	string logName = helper::getNewName("/tmp/" + helper::defaultLogName);
	int logfd = open(logName.c_str(), O_WRONLY | O_CREAT, 0644);

	if (logfd < 0) {
		perror("Failed to create log file");
		exit(1);
	}
#endif

	//create epoll
	int epoll = epoll_create(16); //9 adjacents, log file, the clock, and a few controllers
	if (epoll < 0) {
		perror("Failed to create epoll handle");
		close(controllerfd);
		close(regionfd);
		close(worldviewerfd);
		close(clockfd);
		exit(1);
	}

	// Add clock and client sockets to epoll
	net::EpollConnection
    clockconn(epoll, EPOLLIN, clockfd, net::connection::CLOCK),
    controllerconn(epoll, EPOLLIN, controllerfd, net::connection::CONTROLLER_LISTEN),
    regionconn(epoll, EPOLLIN, regionfd, net::connection::REGION_LISTEN),
    worldviewerconn(epoll, EPOLLIN, worldviewerfd, net::connection::WORLDVIEWER_LISTEN);

	//handle logging to file initializations
	PuckStack puckstack;
	ServerRobot serverrobot;
	ClientRobot clientrobot;

	//server variables
#ifdef ENABLE_LOGGING
	MessageWriter logWriter(logfd);
#endif
	TimestepUpdate timestep;
	TimestepDone tsdone;
	tsdone.set_done(true);
	WorldInfo worldinfo;
	RegionInfo regioninfo;
	unsigned myId = 0; //region id

	//for synchronization
	int round = 0;
	bool sendTsdone = false;

	AreaEngine* regionarea = new AreaEngine(ROBOTDIAMETER,REGIONSIDELEN, MINELEMENTSIZE, VIEWDISTANCE, VIEWANGLE,
			MAXSPEED, MAXROTATE);
	//create robots for benchmarking!
	int numRobots = 0;
	//int wantRobots = 1000;
	int serverByPosition[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	MessageWriter writer(clockfd);
	MessageReader reader(clockfd);
	int timeSteps = 0;
	bool initialized = false;
	time_t lastSecond = time(NULL);
	vector<net::EpollConnection*> controllers;
	vector<net::EpollConnection*> worldviewers;
	vector<net::EpollConnection*> borderRegions;

	//send port listening info (IMPORTANT)
	//add listening ports
	RegionInfo info;
	info.set_address(0);
	info.set_id(0);
	info.set_regionport(regionPort);
	info.set_renderport(worldviewerPort);
	info.set_controllerport(controllerPort);
	clockconn.queue.push(MSG_REGIONINFO, info);
	clockconn.set_writing(true);

#define MAX_EVENTS 128
	struct epoll_event events[MAX_EVENTS];

	//enter the main loop
	while (true) {

		//wait on epoll
		int eventcount;
    do {
      eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
    } while(eventcount < 0 && errno == EINTR);
		if (0 > eventcount) {
			perror("Failed to wait on sockets");
			break;
		}

		//check our events that were triggered
		for (size_t i = 0; i < (unsigned) eventcount; i++) {
      //timestep outputting
      //cache the time, so we don't call it several times
      gettimeofday(&timeCache, NULL);

      //check if its time to output
      if (timeCache.tv_sec > lastSecond) {
        cout << timeSteps/2 << " timesteps/second" << endl;

        timeSteps = 0;
        lastSecond = timeCache.tv_sec;
      }

			net::EpollConnection *c = (net::EpollConnection*) events[i].data.ptr;
			if (events[i].events & EPOLLIN) {
				switch (c->type) {
				case net::connection::CLOCK: {
					//we get a message from the clock server
					MessageType type;
					int len;
					const void *buffer;
					if (c->reader.doRead(&type, &len, &buffer)) {
						switch (type) {
						case MSG_WORLDINFO: {
							worldinfo.ParseFromArray(buffer, len);
							cout << "Got world info." << endl;
							//added pucks to the world
              					unsigned pucks = 0;
              					while(pucks < worldinfo.numpucks() ){// populate the world with numpucks which are randomly distributed over the region
								int a = (2 * ROBOTDIAMETER)+MINELEMENTSIZE + rand() % (REGIONSIDELEN-2*((2 * ROBOTDIAMETER)+MINELEMENTSIZE));
								int b = (2 * ROBOTDIAMETER)+MINELEMENTSIZE + rand() % (REGIONSIDELEN-2*((2 * ROBOTDIAMETER)+MINELEMENTSIZE));
							  regionarea->AddPuck(a, b);
								pucks++;
              }
							int lastRegionIndex = worldinfo.region_size() - 1;
							myId = worldinfo.region(lastRegionIndex).id();
							cout << "My id: " << myId << endl;
              // Draw this:
              // 00 | 01 | 02
              // ---+----+---
              // 07 | ME | 03
              // ---+----+---
              // 06 | 05 | 04
              for (int region = 0; region < worldinfo.region_size() - 1; region++) {
                for (int pos = 0;
                    pos < worldinfo.region(region).position_size(); pos++) {
                  serverByPosition[(int)(worldinfo.region(region).
                      position(pos))] = worldinfo.region(region).id();
                }
              }
              cout << std::setw(2) << std::setfill('0') << serverByPosition[0]
                   << " | " << std::setw(2) << std::setfill('0')
                   << serverByPosition[1] << " | " << std::setw(2)
                   << std::setfill('0') << serverByPosition[2] << endl
                   << "---+----+---\n" << std::setw(2) << std::setfill('0')
                   << serverByPosition[7] << " | ME | " << std::setw(2)
                   << std::setfill('0') << serverByPosition[3]
                   << endl << "---+----+---\n" << std::setw(2)
                   << std::setfill('0') << serverByPosition[6] << " | "
                   << std::setw(2) << std::setfill('0') << serverByPosition[5]
                   << " | " << std::setw(2) << std::setfill('0')
                   << serverByPosition[4] << endl;

							// Connect to existing RegionServers.
							for (int i = 0; i < worldinfo.region_size() - 1; i++) {
								if (worldinfo.region(i).position_size() > 0) {
									struct in_addr addr;
									addr.s_addr = worldinfo.region(i).address();
									int regionfd = net::do_connect(addr, worldinfo.region(i).regionport());
									if (regionfd < 0) {
                   						 throw SystemError("Failed to connect to region server");
									} else if (regionfd == 0) {
										throw runtime_error("Invalid controller address");
									} else {
										cout << "Connected to regionserver" << endl;
									}
									net::set_blocking(regionfd, false);
									net::EpollConnection *newconn = new net::EpollConnection(epoll, EPOLLIN, regionfd,
											net::connection::REGION);
									borderRegions.push_back(newconn);

									//iterate back through the regions we've already done, and dont add if we already have
									bool exists = false;
									for(unsigned k = 0; k < regionsAdded.size(); k++)
									  if(worldinfo.region(i).id() == (unsigned)regionsAdded.at(k))
									  {
									    exists = true;
									    break;
									  }
									if(!exists)
									{
									  uniqueRegions.push_back(pair <int, int> (newconn->fd, -1));
									  regionsAdded.push_back(worldinfo.region(i).id());
								  }

									// Reverse all the positions. If we are connecting to the
									// server on our left, we want to tell it that we are on
									// its right.
									for (int j = 0; j < worldinfo.region(i).position_size(); j++) {
										// Inform AreaEngine who our neighbours are.
										regionarea->SetNeighbour((int) worldinfo.region(i).position(j), newconn);

										worldinfo.mutable_region(i)->set_id(myId);
										// Flip Positions to tell other RegionServers that I am
										// your new neighbour.
										switch (worldinfo.region(i).position(j)) {
										case RegionInfo_Position_TOP_LEFT:
											worldinfo.mutable_region(i)->set_position(j,
													RegionInfo_Position_BOTTOM_RIGHT);
											break;
										case RegionInfo_Position_TOP:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_BOTTOM);
											break;
										case RegionInfo_Position_TOP_RIGHT:
											worldinfo.mutable_region(i)->set_position(j,
													RegionInfo_Position_BOTTOM_LEFT);
											break;
										case RegionInfo_Position_RIGHT:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_LEFT);
											break;
										case RegionInfo_Position_BOTTOM_RIGHT:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_TOP_LEFT);
											break;
										case RegionInfo_Position_BOTTOM:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_TOP);
											break;
										case RegionInfo_Position_BOTTOM_LEFT:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_TOP_RIGHT);
											break;
										case RegionInfo_Position_LEFT:
											worldinfo.mutable_region(i)->set_position(j, RegionInfo_Position_RIGHT);
											break;
										default:
											cout << "Some issue with Position flipping\n";
											break;
										}
									}
									newconn->queue.push(MSG_REGIONINFO, worldinfo.region(i));
									newconn->set_writing(true);
								}
							}

							HomeInfo *homeinfo;
							//handle the homes
							for(int i = 0; i < worldinfo.home_size(); i++ )
							{
								homeinfo=worldinfo.mutable_home(i);
								if (homeinfo->region_id() == myId )
								{
									myHomes.push_back(homeinfo);
									cout<<"Tracking home at ("+helper::toString(homeinfo->home_x())+", "+helper::toString(homeinfo->home_y())+")"<<endl;
								}
							}

							//send the timestepdone packet to tell the clock server we're ready
							writer.init(MSG_TIMESTEPDONE, tsdone);
							for (bool complete = false; !complete;) {
								complete = writer.doWrite();
							}

							//NOW you can start the clock
							cout << "Connected to neighbours! Ready for simulation to begin." << endl;

							break;
						}
						case MSG_REGIONINFO: {
							regioninfo.ParseFromArray(buffer, len);
							cout << "Got region info from ClockServer." << endl;
							break;
						}
						case MSG_TIMESTEPUPDATE: {
							timestep.ParseFromArray(buffer, len);
							//do our initializations here in the init step
							if(!initialized)
							{
                //ready the engine buffers
                regionarea->clearBuffers();

								// Find our robots, and add to the simulation
							  vector<int> myRobotIds;
							  vector<int> myRobotTeams;
							  numRobots = 0;
							  for (google::protobuf::RepeatedPtrField<const RobotInfo>::const_iterator i =
									  worldinfo.robot().begin(); i != worldinfo.robot().end(); i++) {
								  if (i->region() == myId) {
									  myRobotIds.push_back(i->id());
									  myRobotTeams.push_back(i->team());

									  //and add the robot! It's THAT easy!
									  regionarea->AddRobot(i->id(), i->x()+MINELEMENTSIZE, i->y()+MINELEMENTSIZE, 0, 0, 0, 0, i->team(), true);
									  numRobots++;
								  }
							  }

                //tell the engine to send its buffer contents
                regionarea->flushBuffers();

							  cout << numRobots << " robots created." << endl;

							  //regionarea->flushNeighbours();
							  round++;  //this is the async replacement
							  sendTsdone = true;

							  initialized = true;

							  break;
						  }

							generateImage = false;
							//find out if we even need to generate an image
							//generate an image every ( 50 milliseconds ) if we can
							if ( (regionarea->curStep%2==1) && (timeCache.tv_sec*1000000 + timeCache.tv_usec) > (microTimeCache.tv_sec*1000000 + microTimeCache.tv_usec)+50000) {
								for (vector<net::EpollConnection*>::const_iterator it = worldviewers.begin(); it
										!= worldviewers.end(); ++it) {
									if (sendMoreWorldViews[(*it)->fd].initialized == true
											&& sendMoreWorldViews[(*it)->fd].value == true) {
										generateImage = true;
										microTimeCache = timeCache;
										break;
									}
								}
							}

							regionarea->Step(generateImage);

              //async 'flush'
              round++;  //we need to ensure we get all neighbour data before continuing
              sendTsdone = true;

							timeSteps++; //Note: only use this for this temp stat taking. use regionarea->curStep for syncing

							if (generateImage) {
								for (vector<net::EpollConnection*>::const_iterator it = worldviewers.begin(); it
										!= worldviewers.end(); ++it) {
									if (sendMoreWorldViews[(*it)->fd].initialized
											&& sendMoreWorldViews[(*it)->fd].value) {
										(*it)->queue.push(MSG_REGIONVIEW, regionarea->render);
										(*it)->set_writing(true);
									}
								}

							}

#ifdef ENABLE_LOGGING
							logWriter.init(MSG_TIMESTEPUPDATE, timestep);
							logWriter.doWrite();

							for (int i = 0; i < 50; i++) {
								serverrobot.set_id(rand() % 1000 + 1);
								logWriter.init(MSG_SERVERROBOT, serverrobot);
								for (bool complete = false; !complete;) {
									complete = logWriter.doWrite();
									;
								}
							}

							for (int i = 0; i < 25; i++) {
								puckstack.set_stacksize(rand() % 1000 + 1);
								logWriter.init(MSG_PUCKSTACK, puckstack);
								for (bool complete = false; !complete;) {
									complete = logWriter.doWrite();
									;
								}
							}
#endif
							bool ready = true;
			        for(unsigned i = 0; i < uniqueRegions.size(); i++)
			          if(uniqueRegions.at(i).second != regionarea->curStep)
			          {
			            ready = false;
			            break;
		            }

	            if((ready && sendTsdone)){
                //Respond with done message
                clockconn.queue.push(MSG_TIMESTEPDONE, tsdone);
                clockconn.set_writing(true);
                sendTsdone = false;
              }

							break;
						}
						default:
							cerr << "Unexpected readable socket!" << endl;
							break;
						}
					}
					break;
				}
				case net::connection::CONTROLLER: {
					MessageType type;
					int len;
					const void *buffer;
					if (c->reader.doRead(&type, &len, &buffer)) {
						switch (type) {
						case MSG_CLIENTROBOT:
						{
							clientrobot.ParseFromArray(buffer, len);

							if(regionarea->WeControlRobot(clientrobot.id()))
							{
							  if (clientrobot.has_puckpickup()) {
                  if (clientrobot.puckpickup()) {
                    regionarea->PickUpPuck(clientrobot.id());
                  } else {
                    regionarea->DropPuck(clientrobot.id());
                  }
                }

							  regionarea->ChangeVelocity(clientrobot.id(),
                    clientrobot.velocityx(), clientrobot.velocityy());

							}else{
							  //bounced

                BouncedRobot bouncedrobot;
                ClientRobot* newClientRobot = bouncedrobot.mutable_clientrobot();
                newClientRobot->CopyFrom(clientrobot);
                bouncedrobot.set_bounces(1);

                c->queue.push(MSG_BOUNCEDROBOT, bouncedrobot);
                c->set_writing(true);

              }

              // TODO: Combine angle and velocity into ChangeState, or
              // something to that effect.
							//regionarea->ChangeAngle(clientrobot.id(), clientrobot.angle());

							break;
						}
            case MSG_BOUNCEDROBOT:
            {
              BouncedRobot bouncedrobot;
              bouncedrobot.ParseFromArray(buffer, len);
              if (regionarea->WeControlRobot(bouncedrobot.clientrobot().id())) {
                // Robot is ours. Process clientrobot message.
                // TODO: Change angle/state too
                regionarea->ChangeVelocity(bouncedrobot.clientrobot().id(),
                    bouncedrobot.clientrobot().velocityx(),
                    bouncedrobot.clientrobot().velocityy());
              } else {
                // Not our robot. Tell the controller.
                bouncedrobot.set_bounces(bouncedrobot.bounces() + 1);
                c->queue.push(MSG_BOUNCEDROBOT, bouncedrobot);
                c->set_writing(true);
              }

              break;
            }
						default:
							cerr << "Unexpected readable message from Controller\n";
							break;
						}
					}

					break;
				}
				case net::connection::REGION: {
					MessageType type;
					int len;
					const void *buffer;
					if (c->reader.doRead(&type, &len, &buffer)) {
						switch (type) {
			        case MSG_REGIONUPDATE: {
			          RegionUpdate newUpdate;
			          newUpdate.ParseFromArray(buffer, len);
			          for(int i = 0; i < newUpdate.serverrobot_size(); i++)
  			          regionarea->GotServerRobot(newUpdate.serverrobot(i));

  			        for(unsigned i = 0; i < uniqueRegions.size(); i++)
  			        {  if(uniqueRegions.at(i).first == c->fd)
  			          {
  			            uniqueRegions.at(i).second = newUpdate.timestep();
  			            break;
  			          }
			          }

  			        bool ready = true;
  			        for(unsigned i = 0; i < uniqueRegions.size(); i++)
  			          if(uniqueRegions.at(i).second != regionarea->curStep)
  			          {
  			            ready = false;
  			            break;
			            }

		            if((ready && sendTsdone)){
                  //Respond with done message
	                clockconn.queue.push(MSG_TIMESTEPDONE, tsdone);
	                clockconn.set_writing(true);
	                sendTsdone = false;
                }

			          break;
			        }
			        case MSG_PUCKSTACK: {
			          PuckStack puckstack;
			          puckstack.ParseFromArray(buffer, len);
			          regionarea->GotPuckStack(puckstack);
			          break;
			        }
						case MSG_REGIONINFO: {
							regioninfo.ParseFromArray(buffer, len);
							cout << "Found new neighbour, server #" << regioninfo.id() << endl;
							for (int i = 0; i < regioninfo.position_size(); i++) {
                serverByPosition[(int)(regioninfo.position(i))] = regioninfo.id();

								// Inform AreaEngine of our new neighbour.
								regionarea->SetNeighbour((int) regioninfo.position(i), c);
							}

              bool exists = false;
							for(unsigned k = 0; k < regionsAdded.size(); k++)
							  if(regioninfo.id() == (unsigned)regionsAdded.at(k))
							  {
							    exists = true;
							    break;
							  }
							if(!exists)
							{
							  uniqueRegions.push_back(pair <int, int> (c->fd, -1));
							  regionsAdded.push_back(regioninfo.id());
						  }

              cout << std::setw(2) << std::setfill('0') << serverByPosition[0]
                   << " | " << std::setw(2) << std::setfill('0')
                   << serverByPosition[1] << " | " << std::setw(2)
                   << std::setfill('0') << serverByPosition[2] << endl
                   << "---+----+---\n" << std::setw(2) << std::setfill('0')
                   << serverByPosition[7] << " | ME | " << std::setw(2)
                   << std::setfill('0') << serverByPosition[3]
                   << endl << "---+----+---\n" << std::setw(2)
                   << std::setfill('0') << serverByPosition[6] << " | "
                   << std::setw(2) << std::setfill('0') << serverByPosition[5]
                   << " | " << std::setw(2) << std::setfill('0')
                   << serverByPosition[4] << endl;

              //we're going to do this shit asynchronously, dammit!
              //c->set_reading(false);
              //c->set_writing(false);

							break;
						}
						default:
							cerr << "Unexpected readable message from Region\n";
							break;
						}
					}
					break;
				}
				case net::connection::WORLDVIEWER: {
					//check for a message that disables the sending of data to the worldviewer
					MessageType type;
					int len;
					const void *buffer;

					try {
						if (c->reader.doRead(&type, &len, &buffer)) {
							switch (type) {
							case MSG_SENDMOREWORLDVIEWS: {
								SendMoreWorldViews doWeSend;
								doWeSend.ParseFromArray(buffer, len);

								sendMoreWorldViews[c->fd].value = doWeSend.enable();
								sendMoreWorldViews[c->fd].initialized = true;
								cout << "Setting sendMoreWorldViews for fd " << c->fd << " to " << doWeSend.enable()
										<< endl<<endl;
								break;
							}
							default:
								cerr << "Unexpected readable message from WorldViewer\n";
								break;
							}

						}
					} catch (EOFError e) {
						close(c->fd);
						sendMoreWorldViews[c->fd].value = false;
						cout << "world viewer with fd=" << c->fd << " disconnected" << endl;

						// Remove from sets
						worldviewers.erase(find(worldviewers.begin(), worldviewers.end(), c));
						delete c;
					}

					break;
				}
				case net::connection::REGION_LISTEN: {
					int fd = accept(c->fd, NULL, NULL);
					if (fd < 0) {
						throw SystemError("Failed to accept regionserver");
					}
					net::set_blocking(fd, false);
					try {
						net::EpollConnection *newconn = new net::EpollConnection(epoll, EPOLLIN, fd,
								net::connection::REGION);
						borderRegions.push_back(newconn);
					} catch (SystemError e) {
						cerr << e.what() << endl;
						return;
					}

					cout << "Got regionserver connection." << endl;

					break;
				}
				case net::connection::CONTROLLER_LISTEN: {
					int fd = accept(c->fd, NULL, NULL);
					if (fd < 0) {
						throw SystemError("Failed to accept controller");
					}
					net::set_blocking(fd, false);
					try {
						net::EpollConnection *newconn = new net::EpollConnection(epoll, EPOLLIN, fd,
								net::connection::CONTROLLER);
						controllers.push_back(newconn);
						regionarea->AddController(newconn);
					} catch (SystemError e) {
						cerr << e.what() << endl;
						return;
					}

					cout << "Got controller connection." << endl;
					break;
				}
				case net::connection::WORLDVIEWER_LISTEN: {
					//We failed to read a message, so it's a new connection from the worldviewer
					int fd = accept(c->fd, NULL, NULL);
					if (fd < 0) {
						throw SystemError("Failed to accept world viewer");
					}
					net::set_blocking(fd, false);

					try {
						net::EpollConnection *newconnOut = new net::EpollConnection(epoll, EPOLLIN, fd,
								net::connection::WORLDVIEWER);
						worldviewers.push_back(newconnOut);
						sendMoreWorldViews[fd].value = false;
						sendMoreWorldViews[fd].initialized = true;
					} catch (SystemError e) {
						cerr << e.what() << endl;
						return;
					}

					cout << "Got world viewer connection." << endl;

					break;
				}
				default:
					cerr << "Internal error: Got unhandled readable connection type " << c->type << endl;
					break;

				}
			} else if (events[i].events & EPOLLOUT) {
				switch (c->type) {
				case net::connection::WORLDVIEWER:
					try {
						if (c->queue.doWrite()) {
							c->set_writing(false);
						}
					} catch (runtime_error e) {
						close(c->fd);
						sendMoreWorldViews[c->fd].value = false;
						cout << "world viewer with fd=" << c->fd << " disconnected" << endl;

						// Remove from sets
						worldviewers.erase(find(worldviewers.begin(), worldviewers.end(), c));
						delete c;
					}
					break;
				case net::connection::REGION:
					// fall through...
				case net::connection::CONTROLLER:
					// fall through...
				case net::connection::CLOCK:
					if (c->queue.doWrite()) {
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
	shutdown(worldviewerfd, SHUT_RDWR);
	close(worldviewerfd);
}

//this is the main loop for the server
int main(int argc, char* argv[]) {
	//Print a starting message
	printf("--== Region Server Software ==-\n");

	////////////////////////////////////////////////////
	printf("Server Initializing ...\n");

	helper::CmdLine cmdline(argc, argv);
	configFileName = cmdline.getArg("-c", "config").c_str();
	cout << "Using config file: " << configFileName << endl;

	loadConfigFile();
  if(cmdline.getArg("-l").length()) {
    strncpy(clockip, cmdline.getArg("-l").c_str(), 40);
  }
  cout << "Using clock IP: " << clockip << endl;
	////////////////////////////////////////////////////

	printf("Server Running!\n");

	run();

	printf("Server Shutting Down ...\n");

	printf("Goodbye!\n");

	return 0;
}
