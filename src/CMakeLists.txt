add_executable(server server.c tcp/tcp_server.c)
add_executable(client client.c tcp/tcp_client.c)
add_executable(judge judge/judge.c)