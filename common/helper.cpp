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

	Config::Config(int argc, char* argv[])
	{
		//loop through the arguments and parse them into a map
		for(int i = 1; i < argc; i++)
		{
			//assume that all arguments are provide in "-argType arg" type of way
			if( i % 2 )
			{
				Config::parsedConfig[argv[i]]="";
				//cout<<"put " << argv[i] <<" into key"<<endl;
			}
			else
			{
				Config::parsedConfig[argv[i-1]]=argv[i];
				//cout<<"put " << argv[i] <<" into key of "<<argv[i-1]<<endl;
			}
		}
	}

	//return the value of the argument with the type "arg"
	string Config::getArg(string arg)
	{
		return Config::parsedConfig[arg];
	}

}
