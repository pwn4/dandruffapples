#ifndef _EXCEPT_H_
#define _EXCEPT_H_

#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <string>

class SystemError : public std::runtime_error {
protected:
  int _number;
public:
  SystemError() : runtime_error(strerror(errno)), _number(errno) {}
  SystemError(const std::string &msg) : runtime_error((msg + ": ") + strerror(errno)), _number(errno) {}
  SystemError(int errno_) : runtime_error(strerror(errno_)), _number(errno_) {}
  SystemError(int errno_, const std::string &msg) : runtime_error((msg + ": ") + strerror(errno_)), _number(errno_) {}

  int number() { return _number; }
};

class EOFError : public std::runtime_error {
public:
  EOFError() : runtime_error("End of file") {}
};

class UninitializedMessageError : public std::runtime_error {
public:
  UninitializedMessageError() : runtime_error("Attempted to write an uninitialized message!") {}
};

#endif
