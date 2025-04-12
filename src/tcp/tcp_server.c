#include "tcp_server.h"

// server running flag (volatile sig_atomic_t is safe to use in signal handler)
volatile sig_atomic_t server_running = 1;

// linked list of client connections
static client_conn *conn_list = NULL;

/**
 * @brief set the file descriptor to non-blocking mode
 * @param fd file descriptor
 */
static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl(F_GETFL) failed");
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl(F_SETFL) failed");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief add connection to the connection list
 * @param conn client connection
 */
static void add_connection(client_conn *conn)
{
    conn->next = conn_list;
    conn_list = conn;
}

/**
 * @brief remove connection from the connection list
 * @param conn client connection
 */
static void remove_connection(client_conn *conn)
{
    client_conn **p = &conn_list;
    while (*p)
    {
        if (*p == conn)
        {
            *p = conn->next;
            break;
        }
        p = &(*p)->next;
    }
    if (conn->fp)
        fclose(conn->fp);
    if (conn->fd >= 0)
        close(conn->fd);
    if (conn->judge_pipe_fd >= 0)
        close(conn->judge_pipe_fd);
    free(conn);
}

/**
 * @brief spawn judge process
 * @param conn client connection
 */
static void spawn_judge(client_conn *conn)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0)
    {
        perror("pipe failed");
        conn->state = STATE_DONE;
        return;
    }
    set_nonblocking(pipe_fd[0]);
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        conn->state = STATE_DONE;
        return;
    }
    else if (pid == 0)
    {
        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]);

        execl("build/src/judge", "judge", conn->source_filename, (char *)NULL);
        perror("execl failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        conn->judge_pipe_fd = pipe_fd[0];
        close(pipe_fd[1]);
        conn->state = STATE_WAIT_JUDGE;
    }
}

/**
 * @brief read header data from the client
 * @param conn client connection
 */
static void handle_read_header(client_conn *conn)
{
    ssize_t n = recv(conn->fd, conn->header + conn->header_bytes, HEADER_SIZE - conn->header_bytes, 0);
    if (n < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            perror("recv header failed");
            conn->state = STATE_DONE;
        }
        return;
    }
    else if (n == 0)
    {
        conn->state = STATE_DONE;
        return;
    }
    conn->header_bytes += n;
    if (conn->header_bytes == HEADER_SIZE)
    {
        uint64_t net_file_size;
        memcpy(&net_file_size, conn->header + 8, 8);
        conn->file_size = be64toh(net_file_size);
        conn->file_received = 0;

        char filename[256];
        snprintf(filename, sizeof(filename), "files/receive/%s_%d_%ld.c",
                 inet_ntoa(conn->addr.sin_addr), ntohs(conn->addr.sin_port), time(NULL));

        strncpy(conn->source_filename, filename, sizeof(conn->source_filename));
        conn->source_filename[sizeof(conn->source_filename) - 1] = '\0';
        conn->fp = fopen(filename, "wb");
        if (!conn->fp)
        {
            perror("fopen failed");
            conn->state = STATE_DONE;
            return;
        }
        conn->state = STATE_READING_FILE;
    }
}

/**
 * @brief read file data from the client
 * @param conn client connection
 */
static void handle_read_file(client_conn *conn)
{
    char buf[BUFFER_SIZE];
    ssize_t n = recv(conn->fd, buf, BUFFER_SIZE, 0);
    if (n < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            perror("recv file failed");
            conn->state = STATE_DONE;
        }
        return;
    }
    else if (n == 0)
    {
        conn->state = STATE_DONE;
        return;
    }
    size_t written = fwrite(buf, 1, n, conn->fp);
    if (written != (size_t)n)
    {
        perror("fwrite failed");
        conn->state = STATE_DONE;
        return;
    }
    conn->file_received += n;
    if (conn->file_received >= conn->file_size)
    {
        fclose(conn->fp);
        conn->fp = NULL;
        spawn_judge(conn);
    }
}

/**
 * @brief read judge result from the pipe
 * @param conn client connection
 */
static void handle_read_judge(client_conn *conn)
{
    char buf[BUFFER_SIZE];
    ssize_t n = read(conn->judge_pipe_fd, buf, sizeof(buf));
    if (n < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            perror("read judge failed");
            conn->state = STATE_DONE;
        }
        return;
    }
    else if (n == 0)
    {
        conn->judge_result[conn->judge_result_len] = '\0';
        conn->state = STATE_SENDING_RESULT;
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0)
            ;
        return;
    }
    if (conn->judge_result_len + n < JUDGE_RESULT_SIZE - 1)
    {
        memcpy(conn->judge_result + conn->judge_result_len, buf, n);
        conn->judge_result_len += n;
    }
    else
    {
        conn->judge_result_len = JUDGE_RESULT_SIZE - 1;
        conn->state = STATE_SENDING_RESULT;
    }
}

/**
 * @brief send judge result to the client
 * @param conn client connection
 */
static void handle_send_result(client_conn *conn)
{
    ssize_t n = send(conn->fd, conn->judge_result + conn->judge_sent, conn->judge_result_len - conn->judge_sent, 0);
    if (n < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            perror("send result failed");
            conn->state = STATE_DONE;
        }
        return;
    }
    conn->judge_sent += n;
    if (conn->judge_sent >= conn->judge_result_len)
    {
        conn->state = STATE_DONE;
    }
}

void sigint_handler(int signum)
{
    (void)signum;       // remove unused warning
    server_running = 0; // set the flag to stop the server
}

void sigchld_handler(int signo)
{
    (void)signo; // remove unused warning
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int start_tcp_server(int port)
{
    signal(SIGCHLD, sigchld_handler);

    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);

    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction SIGINT failed");
        exit(EXIT_FAILURE);
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(listen_fd, BACKLOG) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    set_nonblocking(listen_fd);
    printf("TCP server listening on port %d\n", port);

    fd_set read_fds, write_fds;
    int max_fd;
    while (server_running)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(listen_fd, &read_fds);
        max_fd = listen_fd;

        for (client_conn *conn = conn_list; conn; conn = conn->next)
        {
            if (conn->state == STATE_READING_HEADER || conn->state == STATE_READING_FILE)
            {
                FD_SET(conn->fd, &read_fds);
                if (conn->fd > max_fd)
                    max_fd = conn->fd;
            }
            else if (conn->state == STATE_WAIT_JUDGE)
            {
                FD_SET(conn->judge_pipe_fd, &read_fds);
                if (conn->judge_pipe_fd > max_fd)
                    max_fd = conn->judge_pipe_fd;
            }
            else if (conn->state == STATE_SENDING_RESULT)
            {
                FD_SET(conn->fd, &write_fds);
                if (conn->fd > max_fd)
                    max_fd = conn->fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        if (activity < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select failed");
            break;
        }

        if (FD_ISSET(listen_fd, &read_fds))
        {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
            if (client_fd >= 0)
            {
                set_nonblocking(client_fd);
                client_conn *conn = malloc(sizeof(client_conn));
                if (!conn)
                {
                    perror("malloc failed");
                    close(client_fd);
                    continue;
                }
                memset(conn, 0, sizeof(client_conn));
                conn->fd = client_fd;
                conn->addr = cli_addr;
                conn->state = STATE_READING_HEADER;
                conn->header_bytes = 0;
                conn->file_size = 0;
                conn->file_received = 0;
                conn->fp = NULL;
                conn->judge_pipe_fd = -1;
                conn->judge_result_len = 0;
                conn->judge_sent = 0;
                add_connection(conn);
                printf("New client connected: %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            }
        }

        client_conn *conn = conn_list;
        client_conn *next;
        while (conn)
        {
            next = conn->next;
            if (conn->state == STATE_READING_HEADER && FD_ISSET(conn->fd, &read_fds))
            {
                handle_read_header(conn);
            }
            else if (conn->state == STATE_READING_FILE && FD_ISSET(conn->fd, &read_fds))
            {
                handle_read_file(conn);
            }
            else if (conn->state == STATE_WAIT_JUDGE && FD_ISSET(conn->judge_pipe_fd, &read_fds))
            {
                handle_read_judge(conn);
            }
            else if (conn->state == STATE_SENDING_RESULT && FD_ISSET(conn->fd, &write_fds))
            {
                handle_send_result(conn);
            }
            if (conn->state == STATE_DONE)
            {
                remove_connection(conn);
            }
            conn = next;
        }
    }
    close(listen_fd);
    return 0;
}
