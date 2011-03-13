#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>

using namespace std;

namespace helper {
template <class T>
inline string toString (const T& t){
	stringstream ss;
	ss << t;

	return ss.str();;
}
string getNewName(string);

class CmdLine {
public:
	CmdLine(int, char**);

	//get the value of an argument from the command line
	string getArg(string arg, string defaultVal="", int maxLength=-1);
private:
	map<string, string> parsedCmdLine;

};

const string defaultLogName = "antix_log";
const string worldViewerDebugLogName="/tmp/worldviewer_debug.txt";
const string clientViewerDebugLogName="/tmp/clientviewer_debug.txt";
}

#endif
