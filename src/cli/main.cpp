#include <vector>
#include <string>

namespace doku::text {
int runCli(const std::vector<std::string> &args);
int runCli(int argc, char *argv[]);
}

int main(int argc, char *argv[]) {
  return doku::text::runCli(argc, argv);
}
