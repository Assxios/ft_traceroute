#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <netinet/in.h>

typedef struct
{
	unsigned int max_ttl;
	unsigned char nprobes;

	int family;
	bool debug;
	unsigned int first_ttl;
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
	char *cmd;
	t_addr server_addr;
	unsigned short port;

	int send_sock;
	int recv_sock;

	t_options options;
} t_data;

extern t_data g_data;

#endif
