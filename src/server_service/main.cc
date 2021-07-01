#include "server.h"


int main(int argc, char* argv[]) {
  if(argc < 2)
  {
      std::cout<<"请输入监听PORT\n";
      return 1;
  }
  int port = std::stoi(argv[1]);
  Server ser(port, 5);
  return 0;
}
