#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "functions.h"

int resolve_addr(char *host)
{
	struct addrinfo hints = {0}, *result;
	hints.ai_family = g_data.options.family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(host, NULL, &hints, &result) != 0)
		return -1;

	memcpy(&g_data.server_addr, result->ai_addr, result->ai_addrlen);
	freeaddrinfo(result);
	return 0;
}

void generate_socket()
{
	if (!g_data.options.icmp)
		g_data.send_sock = socket(g_data.server_addr.sa.sa_family, SOCK_DGRAM, 0);
	g_data.recv_sock = socket(g_data.server_addr.sa.sa_family, SOCK_RAW, g_data.server_addr.sa.sa_family == AF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6);
	if ((!g_data.options.icmp && g_data.send_sock < 0) || (g_data.recv_sock < 0))
		error("socket", strerror(errno));

	struct timeval tv = {1, 0};
	if (setsockopt(g_data.recv_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		error("setsockopt", strerror(errno));

	int on = 1;
	if (g_data.server_addr.sa.sa_family != AF_INET && setsockopt(g_data.recv_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) < 0)
		error("setsockopt", strerror(errno));

	if (g_data.options.debug)
	{
		if (!g_data.options.icmp)
			setsockopt(g_data.send_sock, SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0 ? error("setsockopt", strerror(errno)) : 0;
		setsockopt(g_data.recv_sock, SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0 ? error("setsockopt", strerror(errno)) : 0;

	}
}

void update_ttl(unsigned char ttl)
{
	int ret;

	if (!g_data.options.icmp)
		g_data.server_addr.sa.sa_family == AF_INET ? (ret = setsockopt(g_data.send_sock, SOL_IP, IP_TTL, &ttl, sizeof(ttl))) : (ret = setsockopt(g_data.send_sock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)));
	else
		g_data.server_addr.sa.sa_family == AF_INET ? (ret = setsockopt(g_data.recv_sock, SOL_IP, IP_TTL, &ttl, sizeof(ttl))) : (ret = setsockopt(g_data.recv_sock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)));

	if (ret < 0)
		error("setsockopt", strerror(errno));
}
