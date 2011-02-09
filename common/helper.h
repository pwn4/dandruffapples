#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <map>

using namespace std;

namespace helper
{
	//comment to disable generation of debug for certain programs
	#define DEBUG

	template <class T>

	string toString (const T&);
	string getNewName(string);

	class Config
	{
		public:
			Config(int,char**);
			string getArg(string);
		private:
			map<string, string> parsedConfig;

	} ;
}

#endif
