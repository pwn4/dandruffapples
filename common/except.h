#include <stdexcept>
#include <cerrno>
#include <cstring>

// TODO: Store errno
class SystemError : public std::runtime_error {
 public:
  SystemError() : runtime_error(strerror(errno)) {}
  SystemError(int errno_) : runtime_error(strerror(errno_)) {}
};

class EOFError : public std::runtime_error {
 public:
  EOFError() : runtime_error("End of file") {}
};
