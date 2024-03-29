#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>

#include "types.h"

void update_addr(struct sockaddr_storage *dst, struct sockaddr_storage *src, int family)
{
	memcpy(dst, src, sizeof(*src));

	char ip_str[INET6_ADDRSTRLEN];
	family == AF_INET ? inet_ntop(AF_INET, &((struct sockaddr_in *)src)->sin_addr, ip_str, INET_ADDRSTRLEN) : inet_ntop(AF_INET6, &((struct sockaddr_in6 *)src)->sin6_addr, ip_str, INET6_ADDRSTRLEN);

	if (g_data.options.resolve)
	{
		struct hostent *host = family == AF_INET ? gethostbyaddr((char *)&((struct sockaddr_in *)src)->sin_addr, sizeof(struct in_addr), AF_INET) : gethostbyaddr((char *)&((struct sockaddr_in6 *)src)->sin6_addr, sizeof(struct in6_addr), AF_INET6);
		printf(" %s (%s)", host ? host->h_name : ip_str, ip_str);
	}
	else
		printf(" %s", ip_str);
}

int check_packet_icmp(char *data)
{
	struct icmphdr *icmp = (struct icmphdr *)(g_data.server_addr.sa.sa_family == AF_INET ? data + sizeof(struct iphdr) : data);

	if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP6_TIME_EXCEEDED)
	{
		struct icmphdr *icmp_sent;

		if (g_data.server_addr.sa.sa_family == AF_INET)
		{
			struct iphdr *ip = (struct iphdr *)(icmp + 1);
			icmp_sent = (struct icmphdr *)(ip + 1);

			if (ip->protocol != IPPROTO_ICMP)
				return -1;
		}
		else
		{
			struct ip6_hdr *ip = (struct ip6_hdr *)(icmp + 1);
			icmp_sent = (struct icmphdr *)(ip + 1);

			if (ip->ip6_nxt != IPPROTO_ICMPV6)
				return -1;
		};
		icmp->un.echo = icmp_sent->un.echo;
	}

	if (icmp->un.echo.id != htons(getpid()) || icmp->un.echo.sequence != htons(g_data.sequence))
		return -1;

	if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP6_TIME_EXCEEDED)
		return 0;
	if (icmp->type == ICMP_ECHOREPLY || icmp->type == ICMP6_ECHO_REPLY)
		return 1;
	return -1;
}

int check_packet(char *data)
{
	struct icmphdr *icmp;
	struct udphdr *udp;

	if (g_data.server_addr.sa.sa_family == AF_INET)
	{
		icmp = (struct icmphdr *)(data + sizeof(struct iphdr));
		struct iphdr *ip = (struct iphdr *)(icmp + 1);
		udp = (struct udphdr *)(ip + 1);

		if (ip->protocol != IPPROTO_UDP)
			return -1;
	}
	else
	{
		icmp = (struct icmphdr *)data;
		struct ip6_hdr *ip = (struct ip6_hdr *)(icmp + 1);
		udp = (struct udphdr *)(ip + 1);

		if (ip->ip6_nxt != IPPROTO_UDP)
			return -1;
	}

	if (ntohs(udp->uh_dport) != g_data.sequence)
		return -1;

	static unsigned short source_port = 0;
	if (source_port == 0)
		source_port = ntohs(udp->uh_sport);
	if (ntohs(udp->uh_sport) != source_port)
		return -1;

	if ((icmp->type == ICMP_UNREACH && icmp->code == ICMP_UNREACH_PORT) || (icmp->type == ICMP6_DST_UNREACH && icmp->code == ICMP6_DST_UNREACH_NOPORT))
		return 1;
	if ((icmp->type == ICMP_TIME_EXCEEDED && icmp->code == ICMP_EXC_TTL) || (icmp->type == ICMP6_TIME_EXCEEDED && icmp->code == ICMP6_TIME_EXCEED_TRANSIT))
		return 0;
	return -1;
}

int recv_packet(struct sockaddr_storage *from, struct timeval last)
{
	size_t size = sizeof(struct icmphdr) + sizeof(struct udphdr);
	g_data.server_addr.sa.sa_family == AF_INET ? (size += sizeof(struct iphdr) * 2) : (size += sizeof(struct ip6_hdr));

	char packet[size];
	struct iovec iov = {.iov_base = packet, .iov_len = sizeof(packet)};

	struct sockaddr_storage addr = {0};
	char ctrl[sizeof(struct in6_pktinfo *)];
	struct msghdr msg = {.msg_name = &addr, .msg_namelen = sizeof(addr), .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctrl, .msg_controllen = sizeof(ctrl)};

	if (recvmsg(g_data.recv_sock, &msg, 0) < 0)
	{
		printf(" *");
		return 0;
	}

	struct timeval time;
	gettimeofday(&time, NULL);

	int ret;
	if (!g_data.options.icmp)
		ret = check_packet(packet);
	else
		ret = check_packet_icmp(packet);
	if (ret < 0)
		return ret;

	if (g_data.server_addr.sa.sa_family == AF_INET)
		memcpy(&addr, &((struct iphdr *)packet)->saddr, sizeof(struct in_addr));
	else
		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
			if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
				memcpy(&addr, CMSG_DATA(cmsg), sizeof(struct in6_addr *));

	if (memcmp(&addr, from, sizeof(addr)) != 0)
		update_addr(from, &addr, g_data.server_addr.sa.sa_family);

	printf("  %.3f ms", (time.tv_sec - last.tv_sec) * 1000.0 + (time.tv_usec - last.tv_usec) / 1000.0);
	return ret;
}
