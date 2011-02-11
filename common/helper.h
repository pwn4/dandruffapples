#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>

using namespace std;

//comment to disable generation of debug for certain programs
#define DEBUG

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

const string defaultLogName = "antix_log";
}

#endif
