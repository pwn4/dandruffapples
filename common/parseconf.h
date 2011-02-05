#ifndef _PARSECONF_H_
#define _PARSECONF_H_

#include <stdexcept>
#include <string>
#include <map>

class MissingConfValueError : public std::runtime_error {
public:
  MissingConfValueError(const std::string &key) : runtime_error("Key " + key + " is missing a value!") {}
};

typedef std::map<std::string, std::string> conf;

conf parseconf(const char *path);

#endif
