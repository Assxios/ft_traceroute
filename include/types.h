#ifndef TYPES_H
#define TYPES_H

#include <sys/socket.h>

typedef struct
{
	unsigned int max_ttl;
	unsigned char nprobes;
} t_options;

typedef union
{
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_storage ss;
} t_addr;

typedef struct
{
	t_addr server_addr;

	int send_sock;
	int recv_sock;

	unsigned short port;

	t_options options;
} t_data;

#endif
