#include "WebServ.hpp"

int main() {
  WebServ serv;

  if (serv.setup())
    return (1);
  serv.run();
  return (0);
}
