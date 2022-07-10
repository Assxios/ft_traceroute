#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

struct pkt_format
{
	unsigned int id;
	unsigned int seq;
};

int sndsock;
int icmp_sock;

struct sockaddr_in6 from, to = {0};

u_char packet[512]; /* last inbound (icmp) packet */

int wait_for_reply(struct in6_addr *addr)
{
	char cbuf[sizeof(struct in6_pktinfo *)];
	struct iovec iov = {packet, sizeof(packet)};
	struct msghdr msg = {&from, sizeof(from), &iov, 1, cbuf, sizeof(cbuf), 0};

	int cc = recvmsg(icmp_sock, &msg, 0);
	if (cc >= 0)
		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
			if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
				memcpy(addr, CMSG_DATA(cmsg), sizeof(*addr));
	return (cc);
}

int packet_ok(int seq)
{
	struct icmphdr *icmp = (struct icmphdr *)packet;
	if ((icmp->type == ICMP6_TIME_EXCEEDED && icmp->code == ICMP6_TIME_EXCEED_TRANSIT) ||
		icmp->type == ICMP6_DST_UNREACH)
	{
		struct ip6_hdr *ip = (struct ip6_hdr *)(icmp + 1);
		if (ip->ip6_nxt == IPPROTO_UDP)
		{
			struct udphdr *udp = (struct udphdr *)(ip + 1);
			struct pkt_format *pkt = (struct pkt_format *)(udp + 1);
			if (pkt->id == getpid() && pkt->seq == seq)
				return (icmp->type == ICMP6_DST_UNREACH ? icmp->code + 1 : -1);
		}
	}
	return 0;
}

void print(struct sockaddr_in6 *addr)
{
	char ip[NI_MAXHOST];
	inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));

	struct hostent *hp = gethostbyaddr(&addr->sin6_addr, sizeof(addr->sin6_addr), AF_INET6);
	printf(" %s (%s)", hp ? hp->h_name : ip, ip);
}

double deltaT(struct timeval *t1p, struct timeval *t2p)
{
	return (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 + (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
}

int main(int argc, char **argv)
{
	int max_ttl = 30;
	int nprobes = 3;

	if (argc != 2)
	{
		printf("Usage: %s <host>\n", argv[0]);
		return (1);
	}

	if (getuid() != 0)
	{
		printf("You must be root to run this program.\n");
		return (1);
	}

	if (inet_pton(AF_INET6, argv[1], &to.sin6_addr) != 1)
	{
		struct addrinfo hints = {0}, *res;
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;

		if (getaddrinfo(argv[1], NULL, &hints, &res) != 0)
		{
			printf("Could not resolve %s.\n", argv[1]);
			return (1);
		}
		memcpy(&to.sin6_addr, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, sizeof(to.sin6_addr));
	}
	to.sin6_family = AF_INET6;
	to.sin6_port = htons(33434); // port 33434 is used by mtr (traceroute)

	char pa[NI_MAXHOST];
	inet_ntop(AF_INET6, &to.sin6_addr, pa, sizeof(pa));
	printf("traceroute to %s (%s), %d hops max, %ld byte packets\n", argv[1], pa, max_ttl, sizeof(struct pkt_format));

	sndsock = socket(AF_INET6, SOCK_DGRAM, 0);
	icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

	int on = 1;
	setsockopt(icmp_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));

	struct timeval timeout = {1, 0};
	setsockopt(icmp_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	int got_there = 0, seq = 0;
	for (int ttl = 1; ttl <= max_ttl && !got_there; ++ttl)
	{
		setsockopt(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
		printf("%2d ", ttl);

		struct in6_addr lastaddr = {0};

		for (int probe = 0; probe < nprobes; ++probe)
		{
			struct pkt_format pkt = {getpid(), ++seq};

			sendto(sndsock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&to, sizeof(to));

			struct timeval t1, t2;
			gettimeofday(&t1, NULL);

			while (1)
			{
				if (wait_for_reply(&lastaddr) <= 0)
				{
					printf(" *");
					break;
				}
				gettimeofday(&t2, NULL);

				int code;
				if (!(code = packet_ok(seq)))
					continue;

				if (memcmp(&lastaddr, &from.sin6_addr, sizeof(lastaddr)))
				{
					print(&from);
					memcpy(&lastaddr, &from.sin6_addr, sizeof(lastaddr));
				}
				printf("  %g ms", deltaT(&t1, &t2));

				if (code - 1 == ICMP6_DST_UNREACH_NOPORT)
					++got_there;
				break;
			}
		}
		printf("\n");
	}
	return 0;
}
