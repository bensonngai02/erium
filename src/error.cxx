#include "error.h"
#include <string>

[[noreturn]] void error(std::string message) {
    fprintf(stderr, "%s", message.c_str());
    exit(1);
}