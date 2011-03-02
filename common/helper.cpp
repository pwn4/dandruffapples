#include "helper.h"

namespace helper {

string toString(int t) {
	stringstream ss;
	ss << t;

	return ss.str();
}

string getNewName(string base) {
	string name = base;
	struct stat stFileInfo;
	int i = 0;
	int intStat = stat(name.c_str(), &stFileInfo);

	//make sure that the file doesn't exist before we create it
	//if it exists, choose another name
	while (!intStat) {
		name = base + helper::toString(i);
		intStat = stat(name.c_str(), &stFileInfo);
		i++;
	}

	return name;
}

CmdLine::CmdLine(int argc, char* argv[]) {
	//loop through the arguments and parse them into a map
	for (int i = 1; i < argc; i++) {
		//assuming the input is either of the type: "-variable value" or just "-variable"
		if(argv[i][0] == '-')
		{
			if( i+1 < argc && argv[i+1][0] != '-')
			{
				parsedCmdLine[argv[i]] = argv[i+1];
				i++;
			}
			else
				parsedCmdLine[argv[i]] = "true";

		}
	}
}

string CmdLine::getArg(string arg, string defaultVal, int maxLength) {
	string retValue;

	if( parsedCmdLine[arg].empty() )
		retValue=defaultVal;
	else
		retValue=parsedCmdLine[arg];

	if( maxLength != -1 )
		retValue = retValue.substr(0, maxLength);

	return retValue;
}

}
