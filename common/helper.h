#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>
#include <netinet/in.h>

#include "messagequeue.h"
#include "messagereader.h"

using namespace std;

//comment to disable generation of debug for certain programs
#define DEBUG
//#define ENABLE_LOGGING

namespace helper {
template<class T>

string toString(const T&);
string getNewName(string);

class Config {
public:
	Config(int, char**);
	string getArg(string);
private:
	map<string, string> parsedConfig;

};
struct connection {
	enum Type {
		UNSPECIFIED,
		REGION_LISTEN,
		CONTROLLER_LISTEN,
		PNGVIEWER_LISTEN,
		CLOCK,
		CONTROLLER,
		REGION,
		PNGVIEWER
	} type;

	enum State {
		INIT, RUN
	} state;

	int fd;
	in_addr_t addr;
	MessageReader reader;
	MessageQueue queue;

	connection(int fd_) :
		type(UNSPECIFIED), state(INIT), fd(fd_), reader(fd_), queue(fd_) {
	}
	connection(int fd_, Type type_) :
		type(type_), state(INIT), fd(fd_), reader(fd_), queue(fd_) {
	}
};

const string defaultLogName = "antix_log";
}

#endif
