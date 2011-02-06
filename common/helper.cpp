#include "helper.h"

namespace helper
{
	template <class T>

	string toString (const T& t)
	{
		stringstream ss;
		ss << t;

		return ss.str();
	}

	string getNewName(string base)
	{
		string name= base;
		struct stat stFileInfo;
		int i=0;
		int intStat = stat(name.c_str(),&stFileInfo);

		//make sure that the file doesn't exist before we create it
		//if it exists, choose another name
		while(!intStat)
		{
			name = base+helper::toString(i);
			intStat = stat(name.c_str(),&stFileInfo);
			i++;
		}

		return name;
	}
}
