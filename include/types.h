#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>

#define DEFAULT_PACKET_SIZE 28

typedef struct
{
	int family;

	unsigned int first_ttl;
	unsigned int max_ttl;
	unsigned char nprobes;
	unsigned char packetlen;

	bool debug;
	bool icmp;
	bool resolve;
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
	struct icmphdr icmp;
} t_data;

extern t_data g_data;

#endif
