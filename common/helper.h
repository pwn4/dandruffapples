#ifndef _HELPERFN_H_
#define _HELPERFN_H_

#include <sstream>
#include <string>
#include <sys/stat.h>

using namespace std;

namespace helper
{
	template <class T>

	string toString (const T&);
	string getNewName(string);
}

#endif
