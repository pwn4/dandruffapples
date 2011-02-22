#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>
#include <netinet/in.h>

using namespace std;

//comment to disable generation of debug for certain programs
#define DEBUG
//#define ENABLE_LOGGING

namespace helper {

string toString(int);
string getNewName(string);

class Config {
public:
	Config(int, char**);
	string getArg(string);
private:
	map<string, string> parsedConfig;

};

const string defaultLogName = "antix_log";
const string worldViewerDebugLogName="/tmp/worldviewer_debug.txt";
}

#endif
