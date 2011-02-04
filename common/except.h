#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <string>

// TODO: Store errno
class SystemError : public std::runtime_error {
 public:
  SystemError() : runtime_error(strerror(errno)) {}
  SystemError(const std::string &msg) : runtime_error((msg + ": ") + strerror(errno)) {}
  SystemError(int errno_) : runtime_error(strerror(errno_)) {}
  SystemError(int errno_, const std::string &msg) : runtime_error((msg + ": ") + strerror(errno_)) {}
};

class EOFError : public std::runtime_error {
 public:
  EOFError() : runtime_error("End of file") {}
};
