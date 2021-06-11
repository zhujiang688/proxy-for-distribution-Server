#include <unistd.h>
#include <string>

struct address
{
    int port;
    char ip[20];
    address()
    {
        bzero(ip,sizeof(ip));
    }
};