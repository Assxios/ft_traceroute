#include <stdio.h>
#include <sys/time.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>

void update_addr(struct sockaddr_storage *dst, struct sockaddr_storage *src, int family)
{
	char ip_str[INET6_ADDRSTRLEN];
	struct hostent *host;

	if (family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)src;
		inet_ntop(AF_INET, &sin->sin_addr, ip_str, INET_ADDRSTRLEN);
		host = gethostbyaddr((char *)&sin->sin_addr, sizeof(struct in_addr), AF_INET);
	}
	else
	{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)src;
		inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, INET6_ADDRSTRLEN);
		host = gethostbyaddr((char *)&sin6->sin6_addr, sizeof(struct in6_addr), AF_INET6);
	}
	printf(" %s (%s)", host ? host->h_name : ip_str, ip_str);

	memcpy(dst, src, sizeof(*src));
}

int check_packet(char *data, int family, unsigned short port)
{
	struct icmphdr *icmp;
	struct udphdr *udp;

	if (family == AF_INET)
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

	if (ntohs(udp->uh_dport) != port)
		return -1;

	static unsigned short source_port = 0;
	if (source_port == 0)
		source_port = ntohs(udp->uh_sport);
	if (ntohs(udp->uh_sport) != source_port)
		return -1;

	if (family == AF_INET)
	{
		if (icmp->type == ICMP_UNREACH || icmp->code == ICMP_UNREACH_PORT)
			return 1;
		if (icmp->type == ICMP_TIME_EXCEEDED && icmp->code == ICMP_EXC_TTL)
			return 0;
	}
	else
	{
		if (icmp->type == ICMP6_DST_UNREACH || icmp->code == ICMP6_DST_UNREACH_NOPORT)
			return 1;
		if (icmp->type == ICMP6_TIME_EXCEEDED && icmp->code == ICMP6_TIME_EXCEED_TRANSIT)
			return 0;
	}
	return -1;
}

int recv_packet(int socket, int family, unsigned short port, struct sockaddr_storage *from, struct timeval last)
{
	size_t size = sizeof(struct icmphdr) + sizeof(struct udphdr);
	if (family == AF_INET)
		size += sizeof(struct iphdr) * 2;
	else
		size += sizeof(struct ip6_hdr);

	char packet[size];
	struct iovec iov = {.iov_base = packet, .iov_len = sizeof(packet)};

	struct sockaddr_storage addr = {0};
	char ctrl[sizeof(struct in6_pktinfo *)];
	struct msghdr msg = {.msg_name = &addr, .msg_namelen = sizeof(addr), .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctrl, .msg_controllen = sizeof(ctrl)};

	if (recvmsg(socket, &msg, 0) < 0)
	{
		printf(" *");
		return 0;
	}

	struct timeval time;
	gettimeofday(&time, NULL);

	int ret = check_packet(packet, family, port);
	if (ret < 0)
		return ret;

	if (family == AF_INET)
		memcpy(&addr, &((struct iphdr *)packet)->saddr, sizeof(struct in_addr));
	else
		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
			if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
				memcpy(&addr, CMSG_DATA(cmsg), sizeof(struct in6_addr *));

	if (memcmp(&addr, from, sizeof(addr)) != 0)
		update_addr(from, &addr, family);

	printf(" %.3f ms", (time.tv_sec - last.tv_sec) * 1000.0 + (time.tv_usec - last.tv_usec) / 1000.0);
	return ret;
}
