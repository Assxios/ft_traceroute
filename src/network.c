#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/socket.h>

int resolve_addr(char *host, struct sockaddr *addr)
{
	struct addrinfo hints = {0}, *result;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(host, NULL, &hints, &result) != 0)
		return -1;

	memcpy(addr, result->ai_addr, result->ai_addrlen);
	freeaddrinfo(result);
	return 0;
}

void generate_socket(int family, int *send_sock, int *recv_sock)
{
	*send_sock = socket(family, SOCK_DGRAM, 0);
	*recv_sock = socket(family, SOCK_RAW, family == AF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6);
	if (*send_sock < 0 || *recv_sock < 0)
	{
		printf("Could not create socket.\n");
		exit(1);
	}

	struct timeval tv = {1, 0};
	if (setsockopt(*recv_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
		printf("Could not set socket timeout.\n");
		exit(1);
	}

	int on = 1;
	if (family != AF_INET && setsockopt(*recv_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) < 0)
	{
		printf("Could not set recv packet info.\n");
		exit(1);
	}
}

void update_ttl(int socket, int family, unsigned int ttl)
{
	int ret;
	if (family == AF_INET)
		ret = setsockopt(socket, SOL_IP, IP_TTL, &ttl, sizeof(ttl));
	else
		ret = setsockopt(socket, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));

	if (ret < 0)
	{
		printf("Could not set TTL.\n");
		exit(4);
	}
}