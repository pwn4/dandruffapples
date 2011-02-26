/*/////////////////////////////////////////////////////////////////////////////////////////////////
 Regionserver program
 This program communications with clients, controllers, Worldviewers, other regionservers, and clockservers.
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
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
#include <cairo.h>

#include <google/protobuf/message_lite.h>

#include "../common/timestep.pb.h"
#include "../common/net.h"
#include "../common/claim.pb.h"
#include "../common/clientrobot.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
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
	struct timeval timeCache;
	suseconds_t microTimeCache=0;
	bool generateImage;

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
	net::EpollConnection clockconn(epoll, EPOLLIN, clockfd, net::connection::CLOCK), controllerconn(epoll, EPOLLIN,
			controllerfd, net::connection::CONTROLLER_LISTEN), regionconn(epoll, EPOLLIN, regionfd,
			net::connection::REGION_LISTEN), worldviewerconn(epoll, EPOLLIN, worldviewerfd,
			net::connection::WORLDVIEWER_LISTEN);

	//handle logging to file initializations
	PuckStack puckstack;
	ServerRobot serverrobot;
	ClientRobot clientrobot;
	puckstack.set_x(1);
	puckstack.set_y(1);

	cairo_surface_t *surface;

	//server variables
#ifdef ENABLE_LOGGING
	MessageWriter logWriter(logfd);
#endif
	TimestepUpdate timestep;
	TimestepDone tsdone;
	tsdone.set_done(true);
	WorldInfo worldinfo;
	RegionInfo regioninfo;
	unsigned myId; //region id
	//Region Area Variables (should be set by clock server)
	int regionSideLen = 2500;
	int robotDiameter = 4;
	int minElementSize = 25;
	double viewDistance = 20;
	double viewAngle = 360;
	double maxSpeed = 4;
	double maxRotate = 2;
	AreaEngine* regionarea = new AreaEngine(robotDiameter, regionSideLen, minElementSize, viewDistance, viewAngle,
			maxSpeed, maxRotate);
	//create robots for benchmarking!
	int numRobots = 0;
	int wantRobots = 1000;

	MessageWriter writer(clockfd);
	MessageReader reader(clockfd);
	int timeSteps = 0;
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
	//send the timestepdone packet to tell the clock server we're ready
	writer.init(MSG_TIMESTEPDONE, tsdone);
	for (bool complete = false; !complete;) {
		complete = writer.doWrite();
	}

	//enter the main loop
	while (true) {
		//cache the time, so we don't call it several times
		gettimeofday(&timeCache, NULL);

		//check if its time to output
		if (timeCache.tv_sec > lastSecond) {
			cout << timeSteps << " timesteps/second" << endl;

			timeSteps = 0;
			lastSecond = timeCache.tv_sec;
		}

		//wait on epoll
		int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
		if (0 > eventcount) {
			perror("Failed to wait on sockets");
			break;
		}

		//check our events that were triggered
		for (size_t i = 0; i < (unsigned) eventcount; i++) {
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
							for (int region = 0; region < worldinfo.region_size(); region++) {
								for (int pos = 0; pos < worldinfo.region(region).position_size(); pos++) {
									cout << "Server #" << worldinfo.region(region).id() << " position: ";
									cout << worldinfo.region(region).position(pos) << endl;
								}
							}

							// Connect to existing RegionServers.
							for (int i = 0; i < worldinfo.region_size() - 1; i++) {
								if (worldinfo.region(i).position_size() > 0) {
									struct in_addr addr;
									addr.s_addr = worldinfo.region(i).address();
									int regionfd = net::do_connect(addr, worldinfo.region(i).regionport());
									net::set_blocking(regionfd, false);
									if (regionfd < 0) {
										cout << "Failed to connect to regionserver.\n";
									} else if (regionfd == 0) {
										cout << "Invalid regionserver address.\n";
									} else {
										cout << "Connected to regionserver" << endl;
									}
									net::EpollConnection *newconn = new net::EpollConnection(epoll, EPOLLIN, regionfd,
											net::connection::REGION);
									borderRegions.push_back(newconn);

									// Reverse all the positions. If we are connecting to the
									// server on our left, we want to tell it that we are on
									// its right.
									for (int j = 0; j < worldinfo.region(i).position_size(); j++) {
										// Inform AreaEngine who our neighbours are.
										regionarea->SetNeighbour((int) worldinfo.region(i).position(j), newconn);

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

							// Find our robots, and add to the simulation
							int lastRegionIndex = worldinfo.region_size() - 1;
							myId = worldinfo.region(lastRegionIndex).id();
							vector<int> myRobotIds;
							vector<int> myRobotTeams;
							cout << "My id: " << myId << endl;
							for (google::protobuf::RepeatedPtrField<const RobotInfo>::const_iterator i =
									worldinfo.robot().begin(); i != worldinfo.robot().end(); i++) {
								if (i->region() == myId) {
									myRobotIds.push_back(i->id());
									myRobotTeams.push_back(i->team());
								}
							}
							wantRobots = myRobotIds.size();
							numRobots = 0;
							int rowCounter = 0;
							bool firstrow = true;
							//add some test pucks for now
							regionarea->AddPuck((2 * robotDiameter)+minElementSize+104, (2 * robotDiameter)+minElementSize+110);
							regionarea->AddPuck((2 * robotDiameter)+minElementSize+204, (2 * robotDiameter)+minElementSize+130);
							regionarea->AddPuck((2 * robotDiameter)+minElementSize+304, (2 * robotDiameter)+minElementSize+150);
							//j is the y, i is the x

							for (int j = (2 * robotDiameter)+minElementSize; j < (regionSideLen-minElementSize) - (2 * (robotDiameter)) && numRobots	< wantRobots; j += 4 * (robotDiameter)) {
								for (int i = (2 * robotDiameter)+minElementSize; i < (regionSideLen-minElementSize) - (2 * (robotDiameter)) && numRobots	< wantRobots; i += 5 * (robotDiameter)){
									if(rowCounter == 0)
									  regionarea->AddRobot(myRobotIds[numRobots], i, j, 0, 0, -.5, 0, myRobotTeams[numRobots], true);
									else if(rowCounter == 1)
									  regionarea->AddRobot(myRobotIds[numRobots], i, j, 0, 0, .5, 0, myRobotTeams[numRobots], true);
									else if(firstrow){
									  regionarea->AddRobot(myRobotIds[numRobots], i, j, 0, 0, (double)((rand() % 101)-50.0)/100.0, 0, myRobotTeams[numRobots], true);
									}

									numRobots++;
									rowCounter = (rowCounter+1) % 3;
								}
								firstrow = true;
								rowCounter = 0;
							}

							cout << numRobots << " robots created." << endl;

							break;
						}
						case MSG_REGIONINFO: {
							regioninfo.ParseFromArray(buffer, len);
							cout << "Got region info from ClockServer." << endl;
							break;
						}
						case MSG_TIMESTEPUPDATE: {
							timestep.ParseFromArray(buffer, len);

							generateImage = false;
							//find out if we even need to generate an image
							//generate an image every 100 000  micro seconds ( 10 milliseconds ) if we can
							if ((timeCache.tv_usec/100000) > (microTimeCache/100000) ) {
								for (vector<net::EpollConnection*>::const_iterator it = worldviewers.begin(); it
										!= worldviewers.end(); ++it) {
									if (sendMoreWorldViews[(*it)->fd].initialized == true
											&& sendMoreWorldViews[(*it)->fd].value == true) {
										generateImage = true;
										break;
									}
								}
							}
							microTimeCache = timeCache.tv_usec;

							regionarea->Step(generateImage);

							timeSteps++; //Note: only use this for this temp stat taking. use regionarea->curStep for syncing

							if (generateImage) {
								/*unsigned char *surfaceData = cairo_image_surface_get_data(surface);
								size_t surfaceLength = cairo_image_surface_get_height(surface)
										* cairo_format_stride_for_width(cairo_image_surface_get_format(surface),
												cairo_image_surface_get_width(surface));*/

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
							//Respond with done message
							c->queue.push(MSG_TIMESTEPDONE, tsdone);
							c->set_writing(true);
							break;
						}
						default:
							cerr << "Unexpected readable socket!" << endl;
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
							clientrobot.ParseFromArray(buffer, len);
							if (!regionarea->ChangeVelocity(clientrobot.id(), 
                    clientrobot.velocityx(), clientrobot.velocityy())) {
                // Not my robot!
                BouncedRobot bouncedrobot;
                ClientRobot* newClientRobot = bouncedrobot.mutable_clientrobot();
                newClientRobot->CopyFrom(clientrobot);
                bouncedrobot.set_bounces(1);

                c->queue.push(MSG_BOUNCEDROBOT, bouncedrobot);
                c->set_writing(true);
              }
              // TODO: Combine angle and velocity into ChangeState, or 
              // something to that effect.
							regionarea->ChangeAngle(clientrobot.id(), clientrobot.angle());

							break;
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
                /*
                if (bouncedrobot.bounces() == 2) {
                  cout << "Sending bounce claim: ID #" 
                       << bouncedrobot.clientrobot().id() << endl;
                  // Message was broadcast to all. Let's claim the robot.
                  Claim claim;
                  claim.set_id(bouncedrobot.clientrobot().id());
                  // Send claim to all controllers
                  vector<net::EpollConnection*>::iterator it;
                  vector<net::EpollConnection*>::iterator last = 
                      controllers.end();
                  for (it = controllers.begin(); it != last; it++) {
                    (*it)->queue.push(MSG_CLAIM, claim);
                    (*it)->set_writing(true);
                  }
                }
                */
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
						case MSG_SERVERROBOT: {
							ServerRobot serverrobot;
							serverrobot.ParseFromArray(buffer, len);
							regionarea->GotServerRobot(serverrobot);
							break;
						}
						case MSG_REGIONINFO: {
							regioninfo.ParseFromArray(buffer, len);
							cout << "Found new neighbour, server #" << regioninfo.id() << endl;
							for (int i = 0; i < regioninfo.position_size(); i++) {
								cout << "  Position: " << regioninfo.position(i) << endl;

								// Inform AreaEngine of our new neighbour.
								regionarea->SetNeighbour((int) regioninfo.position(i), c);
							}
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
										<< endl;
								break;
							}
							default:
								cerr << "Unexpected readable message from WorldViewer\n";
							}

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
						sendMoreWorldViews[fd].value = true;
						sendMoreWorldViews[fd].initialized = true;
						cout << "set " << fd << " to true" << endl;
					} catch (SystemError e) {
						cerr << e.what() << endl;
						return;
					}

					cout << "Got world viewer connection." << endl;

					break;
				}
				default:
					cerr << "Internal error: Got unhandled readable connection type " << c->type << endl;

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

	helper::Config config(argc, argv);
	configFileName = (config.getArg("-c").length() == 0 ? "config" : config.getArg("-c").c_str());
	cout << "Using config file: " << configFileName << endl;

	loadConfigFile();
	////////////////////////////////////////////////////

	printf("Server Running!\n");

	run();

	printf("Server Shutting Down ...\n");

	printf("Goodbye!\n");
}
