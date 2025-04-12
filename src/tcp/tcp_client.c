
#include "tcp_client.h"

ssize_t send_all(int sockfd, const void *buffer, size_t length)
{
    size_t total_sent = 0;
    const char *buf = (const char *)buffer;
    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, buf + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            perror("send failed");
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

int connect_to_server(const char *server_ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton failed");
        close(sockfd);
        return -1;
    }
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int send_file_data(int sockfd, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        perror("fopen failed");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    uint64_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char header[HEADER_SIZE];
    memset(header, 0, HEADER_SIZE);
    memcpy(header, TEXTFILE, 8);
    uint64_t net_file_size = htobe64(file_size);
    memcpy(header + 8, &net_file_size, 8);
    if (send_all(sockfd, header, HEADER_SIZE) != HEADER_SIZE)
    {
        perror("failed to send header");
        fclose(fp);
        return -1;
    }
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        if (send_all(sockfd, buffer, bytes_read) != bytes_read)
        {
            perror("failed to send file");
            fclose(fp);
            return -1;
        }
    }
    printf("file '%s' sent (size: %lu bytes)\n", filename, file_size);
    fclose(fp);
    return 0;
}

int receive_judge_result(int sockfd)
{
    char result_buf[4096];
    size_t total_received = 0;
    ssize_t r;
    while ((r = recv(sockfd, result_buf + total_received, sizeof(result_buf) - total_received - 1, 0)) > 0)
    {
        total_received += r;
        if (total_received >= sizeof(result_buf) - 1)
            break;
    }
    if (r < 0)
    {
        perror("recv failed");
        return -1;
    }
    result_buf[total_received] = '\0';
    printf("Judge result received:\n%s\n", result_buf);
    return 0;
}

void close_connection(int sockfd)
{
    close(sockfd);
}

int send_file(int sockfd, const char *filename)
{
    if (send_file_data(sockfd, filename) < 0)
    {
        close_connection(sockfd);
        return -1;
    }
    if (receive_judge_result(sockfd) < 0)
    {
        close_connection(sockfd);
        return -1;
    }
    return 0;
}

#ifdef TEST_TCP_CLIENT
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
#endif
