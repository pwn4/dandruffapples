/*/////////////////////////////////////////////////////////////////////////////////////////////////
 Regionserver program
 This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <cairo.h>

#include <google/protobuf/message_lite.h>

#include "../common/timestep.pb.h"
#include "../common/net.h"
#include "../common/clientrobot.pb.h"
#include "../common/serverrobot.pb.h"
#include "../common/puckstack.pb.h"
#include "../common/messagewriter.h"
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
int pngviewerPort = PNG_VIEWER_PORT;
int regionPort = REGIONS_PORT;
////////////////////////////////////////////////////////////

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

	//png viewer listening port
	if (configuration.find("PNGPORT") != configuration.end()) {
		pngviewerPort = strtol(configuration["PNGPORT"].c_str(), NULL, 10);
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
	bool sendMorePngs = false;
	time_t timeCache;

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

	//listen for PNG viewer connections
	int pngfd = net::do_listen(pngviewerPort);
	net::set_blocking(pngfd, false);

	//create a new file for logging
	string logName = helper::getNewName("/tmp/" + helper::defaultLogName);
	int logfd = open(logName.c_str(), O_WRONLY | O_CREAT, 0644);

	if (logfd < 0) {
		perror("Failed to create log file");
		exit(1);
	}

	//create epoll
	int epoll = epoll_create(16); //9 adjacents, log file, the clock, and a few controllers
	if (epoll < 0) {
		perror("Failed to create epoll handle");
		close(controllerfd);
		close(regionfd);
		close(pngfd);
		close(clockfd);
		close(logfd);
		exit(1);
	}

	// Add clock and client sockets to epoll
	net::EpollConnection clockconn(epoll, EPOLLIN, clockfd,
			net::connection::CLOCK), controllerconn(epoll, EPOLLIN,
			controllerfd, net::connection::CONTROLLER_LISTEN), regionconn(
			epoll, EPOLLIN, regionfd, net::connection::REGION_LISTEN), pngconn(
			epoll, EPOLLIN, pngfd, net::connection::PNGVIEWER_LISTEN);

	//handle logging to file initializations
	PuckStack puckstack;
	ServerRobot serverrobot;
	ClientRobot clientrobot;
	puckstack.set_x(1);
	puckstack.set_y(1);

	RegionRender png;
	cairo_surface_t *surface;

	//server variables
	MessageWriter logWriter(logfd);
	TimestepUpdate timestep;
	TimestepDone tsdone;
	tsdone.set_done(true);
	WorldInfo worldinfo;
	RegionInfo regioninfo;
	//Region Area Variables (should be set by clock server)
	int regionSideLen = 2500;
	int robotDiameter = 4;
	int minElementSize = 50;
	double viewDistance = 20;
	double viewAngle = 360;
	double maxSpeed = 4;
	double maxRotate = 2;
	AreaEngine* regionarea = new AreaEngine(robotDiameter, regionSideLen,
			minElementSize, viewDistance, viewAngle, maxSpeed, maxRotate);
	//create robots for benchmarking!
	int numRobots = 0;
	int wantRobots = 1000;
	//regionarea->AddRobot(numRobots++, 10, 10, 0, .1, 0, 0, "red");
	//regionarea->AddRobot(numRobots++, 1800, 10, 0, -.1, 0, 0, "red");

	MessageWriter writer(clockfd);
	MessageReader reader(clockfd);
	int timeSteps = 0;
	time_t lastSecond = time(NULL);
	vector<net::EpollConnection*> controllers;
	vector<net::EpollConnection*> pngviewers;
	vector<net::EpollConnection*> borderRegions;

	//send port listening info (IMPORTANT)
	//add listening ports
	RegionInfo info;
	info.set_address(0);
	info.set_id(0);
	info.set_regionport(regionPort);
	info.set_renderport(pngviewerPort);
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
		timeCache = time(NULL);

		//check if its time to output
		if (timeCache > lastSecond) {
			cout << timeSteps << " timesteps/second";

			//remove me later: temporary message for debugging
			if (sendMorePngs)
				cout << " and we are generating PNGs";
			cout << endl;

			timeSteps = 0;
			lastSecond = timeCache;
		}

		//wait on epoll
		int eventcount = epoll_wait(epoll, events, MAX_EVENTS, -1);
		if (0 > eventcount) {
			perror("Failed to wait on sockets");
			break;
		}

		//check our events that were triggered
		for (size_t i = 0; i < (unsigned) eventcount; i++) {
			net::EpollConnection *c =
					(net::EpollConnection*) events[i].data.ptr;
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
              int myId;
							worldinfo.ParseFromArray(buffer, len);
							cout << "Got world info." << endl;
              for (int region = 0; region < worldinfo.region_size();
                   region++) {
                for (int pos = 0; pos < worldinfo.region(region).position_size();
                    pos++) {
                  cout << "Server #" << worldinfo.region(region).id()
                       << " position: ";
                  cout << worldinfo.region(region).position(pos) << endl;
                }
                if (region == worldinfo.region_size() - 1) {
                  cout << "My id: " << worldinfo.region(region).id() << endl;
                  myId = worldinfo.region(region).id();
                }
              } 

              // Connect to existing RegionServers.
              for (int i = 0; i < worldinfo.region_size() - 1; i++) {
                if (worldinfo.region(i).position_size() > 0) {
                  struct in_addr addr;
                  addr.s_addr = worldinfo.region(i).address();
                  int regionfd = net::do_connect(addr,
                      worldinfo.region(i).regionport());
                  net::set_blocking(regionfd, false);
                  if (regionfd < 0) {
                    cout << "Failed to connect to regionserver.\n";
                  } else if (regionfd == 0) {
                    cout << "Invalid regionserver address.\n";
                  } else {
                    cout << "Connected to regionserver" << endl;
                  }
                  net::EpollConnection *newconn =
                      new net::EpollConnection(epoll, EPOLLIN, regionfd,
                          net::connection::REGION);
                  borderRegions.push_back(newconn);

                  // Reverse all the positions. If we are connecting to the
                  // server on our left, we want to tell it that we are on
                  // its right.
                  for (int j = 0; j < worldinfo.region(i).position_size();
                       j++) {
                    // Inform AreaEngine who our neighbours are.
                    regionarea->SetNeighbour((int)worldinfo.region(i).position(j),
                        newconn);

                    // Flip Positions to tell other RegionServers that I am
                    // your new neighbour.
                    switch (worldinfo.region(i).position(j)) {
                    case RegionInfo_Position_TOP_LEFT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_BOTTOM_RIGHT);
                      break;
                    case RegionInfo_Position_TOP:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_BOTTOM);
                      break;
                    case RegionInfo_Position_TOP_RIGHT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_BOTTOM_LEFT);
                      break;
                    case RegionInfo_Position_RIGHT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_LEFT);
                      break;
                    case RegionInfo_Position_BOTTOM_RIGHT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_TOP_LEFT);
                      break;
                    case RegionInfo_Position_BOTTOM:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_TOP);
                      break;
                    case RegionInfo_Position_BOTTOM_LEFT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_TOP_RIGHT);
                      break;
                    case RegionInfo_Position_LEFT:
                      worldinfo.mutable_region(i)->set_position(j, 
                          RegionInfo_Position_RIGHT);
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
              int firstRobot = myId * wantRobots; 
              for (int i = 400 * robotDiameter; i < regionSideLen - 3 * (robotDiameter)
                  && numRobots < wantRobots; i += 5 * (robotDiameter))
                for (int j = 3 * robotDiameter; j < regionSideLen - 3 * (robotDiameter)
                    && numRobots < wantRobots; j += 5 * (robotDiameter))
                  regionarea->AddRobot(numRobots++ + firstRobot, i, j, 0, .1, 0, 0, (numRobots % 3
                      == 0 ? "red"
                      : ((numRobots + 1) % 3 == 0 ? "blue" : "green")));
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

							if (regionarea->curStep % 20 == 0 && sendMorePngs)
								regionarea->Step(true);
							else
								regionarea->Step(false);

							timeSteps++; //Note: only use this for this temp stat taking. use regionarea->curStep for syncing

							if (regionarea->curStep % 20 == 0 && sendMorePngs) {
								// Only generate an image for one in 20 timesteps
								surface = regionarea->stepImage;
								unsigned char *surfaceData =
										cairo_image_surface_get_data(surface);
								size_t
										surfaceLength =
												cairo_image_surface_get_height(
														surface)
														* cairo_format_stride_for_width(
																cairo_image_surface_get_format(
																		surface),
																cairo_image_surface_get_width(
																		surface));

								png.set_image((void*) surfaceData,
										surfaceLength);
								png.set_timestep(timestep.timestep());
								for (vector<net::EpollConnection*>::iterator i =
										pngviewers.begin(); i
										!= pngviewers.end(); ++i) {
									(*i)->queue.push(MSG_REGIONRENDER, png);
									(*i)->set_writing(true);
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
							cout
									<< "Received ClientRobot message with robotId #"
									<< clientrobot.id() << endl;

							// Send back a test ServerRobot message. May need to
							// eventually broadcast to all controllers?
							serverrobot.set_id(clientrobot.id());
							c->queue.push(MSG_SERVERROBOT, serverrobot);
							c->set_writing(true);

							break;
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
					  
            case MSG_REGIONINFO: 
            {
              regioninfo.ParseFromArray(buffer, len);
              cout << "Hey bro! Server #" << regioninfo.id()
                   << " is trying to tell us he's our neighbour! He be here:\n";
              for (int i = 0; i < regioninfo.position_size(); i++) {
                cout << "  Position: " << regioninfo.position(i) << endl;
                
                // Inform AreaEngine of our new neighbour.
                regionarea->SetNeighbour((int)regioninfo.position(i), c);
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
				case net::connection::REGION_LISTEN: {
					int fd = accept(c->fd, NULL, NULL);
					if (fd < 0) {
						throw SystemError("Failed to accept regionserver");
					}
					net::set_blocking(fd, false);
					try {
						net::EpollConnection *newconn =
								new net::EpollConnection(epoll, EPOLLIN, fd,
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
						net::EpollConnection *newconn =
								new net::EpollConnection(epoll, EPOLLIN, fd,
										net::connection::CONTROLLER);
						controllers.push_back(newconn);
					} catch (SystemError e) {
						cerr << e.what() << endl;
						return;
					}

					cout << "Got controller connection." << endl;
					break;
				}
				case net::connection::PNGVIEWER_LISTEN: {
					//we get a message from the pngviewer server
					MessageType type;
					int len;
					const void *buffer;

					//check for a message that disables the sending of PNGs to the pngviewer
					try {
						//first check if we are receiving an actual message or a new connection
						if (c->reader.doRead(&type, &len, &buffer)) {
							switch (type) {
							case MSG_SENDMOREPNGS: {
								SendMorePngs doWeSend;
								doWeSend.ParseFromArray(buffer, len);

								sendMorePngs = doWeSend.enable();
							}
							default:
								cerr
										<< "Unexpected message from the pngviewer!"
										<< endl;
								break;
							}
						}
					} catch (SystemError e) {
						//We failed to read a message, so it's a new connection from the pngviewer
						int fd = accept(c->fd, NULL, NULL);
						if (fd < 0) {
							throw SystemError("Failed to accept png viewer");
						}
						net::set_blocking(fd, false);

						try {
							net::EpollConnection *newconn =
									new net::EpollConnection(epoll, EPOLLOUT,
											fd, net::connection::PNGVIEWER);
							pngviewers.push_back(newconn);
							sendMorePngs = true;
						} catch (SystemError e) {
							cerr << e.what() << endl;
							return;
						}

						cout << "Got png viewer connection." << endl;
					}
					break;
				}

				default:
					cerr
							<< "Internal error: Got unexpected readable event of type "
							<< c->type << endl;
					break;
				}
			} else if (events[i].events & EPOLLOUT) {
				switch (c->type) {
				case net::connection::REGION:
          // fall through...
				case net::connection::CONTROLLER:
					// fall through...
				case net::connection::CLOCK:
          if (c->queue.doWrite()) {
            c->set_writing(false);
          }
          break;
				case net::connection::PNGVIEWER:
					// Perform write
					try {
						if (c->queue.doWrite()) {
							// If the queue is empty, we don't care if this is writable
							c->set_writing(false);
						}
					} catch (SystemError e) {
						for (vector<net::EpollConnection*>::iterator i =
								pngviewers.begin(); i != pngviewers.end(); ++i) {
							if (c->fd == (*i)->fd) {
								pngviewers.erase(i);
								close(c->fd);
								cout << "png viewer with fd=" << c->fd
										<< " disconnected" << endl;

								//don't even bother sending more PNGs if we have no one to send them to
								if (pngviewers.size() == 0)
									sendMorePngs = false;

								break;
							}
						}
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
	shutdown(pngfd, SHUT_RDWR);
	close(pngfd);
	close(logfd);
}

//this is the main loop for the server
int main(int argc, char* argv[]) {
	//Print a starting message
	printf("--== Region Server Software ==-\n");

	////////////////////////////////////////////////////
	printf("Server Initializing ...\n");

	helper::Config config(argc, argv);
	configFileName = (config.getArg("-c").length() == 0 ? "config"
			: config.getArg("-c").c_str());
	cout << "Using config file: " << configFileName << endl;

	loadConfigFile();
	////////////////////////////////////////////////////

	printf("Server Running!\n");

	run();

	printf("Server Shutting Down ...\n");

	printf("Goodbye!\n");
}
