#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void check_args(int argc, char **argv);
void resolve_flags(char **argv, struct sockaddr_storage *server_addr, t_options *options);

int resolve_addr(char *host, struct sockaddr_storage *addr);
void generate_socket(int family, int *send_sock, int *recv_sock);
void update_ttl(int socket, int family, unsigned int ttl);

int recv_packet(int socket, int family, unsigned short port, struct sockaddr_storage *from, struct timeval last);
#endif
