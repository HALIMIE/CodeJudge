#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "../defineshit.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>

#define HEADER_SIZE 16
#define TEXTFILE "TEXTFILE"

/**
 * @brief Send all data in the buffer
 * @param sockfd socket file descriptor
 * @param buffer data to send
 * @param length length of the data
 * @return number of bytes sent, or -1 on error
 */
ssize_t send_all(int sockfd, const void *buffer, size_t length);

/**
 * @brief Connect to the server
 * @param server_ip server IP address
 * @param port server port number
 * @return socket file descriptor, or -1 on error
 */
int connect_to_server(const char *server_ip, int port);

/**
 * @brief Send file data to the server
 * @param sockfd socket file descriptor
 * @param filename name of the file to send
 * @return 0 on success, -1 on error
 */
int send_file_data(int sockfd, const char *filename);

/**
 * @brief Receive judge result from the server
 * @param sockfd socket file descriptor
 * @return 0 on success, -1 on error
 */
int receive_judge_result(int sockfd);

/**
 * @brief Close the connection
 * @param sockfd socket file descriptor
 */
void close_connection(int sockfd);

/**
 * @brief Send file to the server and receive judge result
 * @param sockfd socket file descriptor
 * @param filename name of the file to send
 * @return 0 on success, -1 on error
 */
int send_file(int sockfd, const char *filename);

#endif // TCP_CLIENT_H