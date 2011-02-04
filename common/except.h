#include <stdexcept>
#include <cerrno>
#include <cstring>

// TODO: Store errno
class SystemError : public std::runtime_error {
 public:
  SystemError() : runtime_error(strerror(errno)) {}
  SystemError(int errno_) : runtime_error(strerror(errno_)) {}
};

class EOF : public std::runtime_error {
 public:
  EOF() : runtime_error("End of file") {}
};
