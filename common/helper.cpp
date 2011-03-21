#include "helper.h"

namespace helper {
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

//distance between two points
float distanceBetween(float x1, float x2, float y1, float y2) {
	return sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2));
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

//Takes Two Integers and Packs their first 16 bits into 4 bytes.
//Note: Is NOT cross platform ('course neither is epoll)
//ASSUMES BIG ENDIAN, FOR NOW
unsigned int BytePack(int a, int b) {
	unsigned int rtn = a;
	rtn = rtn & bytePackClear;

	unsigned int tmpb = b;
	tmpb = tmpb & bytePackClear;
	tmpb = tmpb << 16;

	rtn = rtn | tmpb;

	return rtn;
}



//ASSUMES BIG ENDIAN, FOR NOW
TwoInt ByteUnpack(unsigned int data) {

	unsigned int first = data & bytePackClear;

	unsigned int second = data >> 16;
	second = second & bytePackClear;

	return TwoInt(first, second);

}
