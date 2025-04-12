#include "tcp/tcp_client.h"
#include "defineshit.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <server_ip> <port> <filename>\n", argv[0]);
        return 1;
    }
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];
    int sockfd = connect_to_server(server_ip, port);
    if (sockfd < 0)
    {
        return 1;
    }
    int ret = send_file(sockfd, filename);
    if (ret < 0)
    {
        return 1;
    }
    close_connection(sockfd);
    return 0;
}