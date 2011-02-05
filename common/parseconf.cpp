#include "parseconf.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "except.h"

using namespace std;

conf parseconf(const char *path) {
  conf res;
  FILE *file = fopen(path, "r");
  if(!file) {
    throw new SystemError("Failed to open config file");
  }

  const char *delims = " \t\r\n";
  char *buffer = NULL, *tok;
  size_t len;
  while(0 < getline(&buffer, &len, file)) {	
    string key;
    tok = strtok(buffer, delims);
    if(!tok) {
      continue;
    }
    key = tok;
    tok = strtok(NULL, delims);
    if(!tok) {
      free(buffer);
      throw MissingConfValueError(key);
    }

    res[key] = tok;
  }

  free(buffer);
  return res;
}
