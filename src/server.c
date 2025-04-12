#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "tcp/tcp_server.h"
#include "defineshit.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    printf("Start TCP server\n");
    start_tcp_server(port);
    printf("TCP server closed, bye\n");
    return 0;
}
