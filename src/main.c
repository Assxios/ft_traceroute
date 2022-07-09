#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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
	unsigned int ident;
	unsigned int seq;
	struct timeval tv;
};

int sndsock;
int icmp_sock;

struct sockaddr_in6 from, to = {0};

u_char packet[512]; /* last inbound (icmp) packet */

void send_probe(int seq, int ttl)
{
	struct pkt_format pkt = {.ident = htonl(getpid()), .seq = htonl(seq)};
	gettimeofday(&pkt.tv, NULL);

	sendto(sndsock, &pkt, sizeof(struct pkt_format), 0, (struct sockaddr *)&to, sizeof(to));
}

int wait_for_reply(struct in6_addr *to)
{
	char cbuf[sizeof(struct in6_pktinfo *)];

	struct iovec iov = {packet, sizeof(packet)};
	struct msghdr msg = {&from, sizeof(from), &iov, 1, cbuf, sizeof(cbuf), 0};

	int cc = recvmsg(icmp_sock, &msg, 0);
	if (cc >= 0)
		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
			if (cmsg->cmsg_level != SOL_IPV6)
				continue;
			switch (cmsg->cmsg_type)
			{
			case IPV6_PKTINFO:
				memcpy(to, CMSG_DATA(cmsg), sizeof(*to));
			}
		}
	return (cc);
}

int packet_ok(int seq, struct timeval *tv)
{
	struct icmphdr *icp = (struct icmphdr *)packet;
	if ((icp->type == ICMP6_TIME_EXCEEDED && icp->code == ICMP6_TIME_EXCEED_TRANSIT) || icp->type == ICMP6_DST_UNREACH)
	{
		struct ip6_hdr *hip = (struct ip6_hdr *)(icp + 1);
		struct udphdr *up = (struct udphdr *)(hip + 1);
		int nexthdr = hip->ip6_nxt;
		if (nexthdr == IPPROTO_FRAGMENT)
			nexthdr = *(unsigned char *)up++;
		if (nexthdr == IPPROTO_UDP)
		{
			struct pkt_format *pkt = (struct pkt_format *)(up + 1);
			if (ntohl(pkt->ident) == getpid() && ntohl(pkt->seq) == seq)
			{
				*tv = pkt->tv;
				return (icp->type == ICMP6_TIME_EXCEEDED ? -1 : icp->code + 1);
			}
		}
	}
	return 0;
}

void print(struct sockaddr_in6 *from)
{
	char ip[NI_MAXHOST];
	inet_ntop(AF_INET6, &from->sin6_addr, ip, sizeof(ip));

	struct hostent *hp = gethostbyaddr(&from->sin6_addr, sizeof(from->sin6_addr), AF_INET6);
	printf(" %s (%s)", hp ? hp->h_name : ip, ip);
}

double deltaT(struct timeval *t1p, struct timeval *t2p)
{
	return (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 + (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: %s <host>\n", argv[0]);
		return (1);
	}

	to.sin6_family = AF_INET6;
	to.sin6_port = htons(33434); // port 33434 is used by mtr (traceroute)

	if (inet_pton(AF_INET6, argv[1], &to.sin6_addr) != 1)
	{
		struct hostent *he = gethostbyname2(argv[1], AF_INET6);
		if (he == NULL)
		{
			printf("Unknown host: %s\n", argv[1]);
			return (1);
		}
		memcpy(&to.sin6_addr, he->h_addr, he->h_length);
	}

	sndsock = socket(AF_INET6, SOCK_DGRAM, 0);
	icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

	int on = 1;
	setsockopt(icmp_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));

	struct timeval timeout = {1, 0};
	setsockopt(icmp_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	int max_ttl = 30;
	int nprobes = 3;

	char pa[NI_MAXHOST];
	fprintf(stderr, "traceroute to %s (%s)", argv[1], inet_ntop(AF_INET6, &to.sin6_addr, pa, sizeof(pa)));
	fprintf(stderr, ", %d hops max, %ld byte packets\n", max_ttl, sizeof(struct pkt_format));

	int seq = 0;
	for (int ttl = 1; ttl <= max_ttl; ++ttl)
	{
		printf("%2d ", ttl);
		setsockopt(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));

		struct in6_addr lastaddr = {0};
		int got_there = 0, unreachable = 0;

		for (int probe = 0; probe < nprobes; ++probe)
		{
			send_probe(++seq, ttl);

			struct timeval t1, t2;
			gettimeofday(&t1, NULL);

			int cc;
			while ((cc = wait_for_reply(&lastaddr)) > 0)
			{
				gettimeofday(&t2, NULL);

				int code;
				if ((code = packet_ok(seq, &t1)))
				{
					if (memcmp(&from.sin6_addr, &lastaddr, sizeof(from.sin6_addr)))
					{
						print(&from);
						memcpy(&lastaddr, &from.sin6_addr, sizeof(lastaddr));
					}
					printf("  %g ms", deltaT(&t1, &t2));

					switch (code - 1)
					{
					case ICMP6_DST_UNREACH_NOPORT:
						++got_there;
						break;
					case ICMP6_DST_UNREACH_NOROUTE:
						++unreachable;
						printf(" !N");
						break;
					case ICMP6_DST_UNREACH_ADDR:
						++unreachable;
						printf(" !H");
						break;
					case ICMP6_DST_UNREACH_ADMIN:
						++unreachable;
						printf(" !S");
						break;
					}
					break;
				}
			}
			if (cc <= 0)
				printf(" *");
		}
		printf("\n");
		if (got_there || (unreachable > 0 && unreachable >= nprobes - 1))
			exit(0);
	}
	return 0;
}
