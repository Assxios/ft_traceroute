#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/if.h>
#if __linux__
#include <endian.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <linux/types.h>
#ifdef CAPABILITIES
#include <sys/capability.h>
#endif
#ifdef USE_IDN
#include <idna.h>
#include <locale.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MAXPACKET 65535
#define MAX_HOSTNAMELEN NI_MAXHOST
#ifndef FD_SET
#define NFDBITS (8 * sizeof(fd_set))
#define FD_SETSIZE NFDBITS
#define FD_SET(n, p) ((p)->fds_bits[(n) / NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p) ((p)->fds_bits[(n) / NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p) ((p)->fds_bits[(n) / NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p) memset((char *)(p), 0, sizeof(*(p)))
#endif
#define Fprintf (void)fprintf
#define Printf (void)printf
u_char packet[512]; /* last inbound (icmp) packet */
int wait_for_reply(int, struct sockaddr_in6 *, struct in6_addr *, int);
int packet_ok(u_char *buf, int cc, struct sockaddr_in6 *from, struct in6_addr *to, int seq, struct timeval *);
void send_probe(int seq, int ttl);
double deltaT(struct timeval *, struct timeval *);
void print(struct sockaddr_in6 *from);
void usage(void);
int icmp_sock;				 /* receive (icmp) socket file descriptor */
int sndsock;				 /* send (udp) socket file descriptor */
struct sockaddr_in6 whereto; /* Who to try to reach */
struct sockaddr_in6 saddr;
char *hostname;
int nprobes = 3;
int max_ttl = 30;
pid_t ident;
u_short port = 32768 + 666; /* start udp dest port # for probe packets */
int options;				/* socket options */
int verbose;
int waittime = 5; /* time to wait for response (in seconds) */
int nflag;		  /* print addresses numerically */
struct pkt_format
{
	__u32 ident;
	__u32 seq;
	struct timeval tv;
};
int datalen = sizeof(struct pkt_format);

int main(int argc, char *argv[])
{
	ident = getpid();
	memset((char *)&whereto, 0, sizeof(whereto));

	if (argc != 2)
		usage();

	icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

	struct sockaddr_in6 from, *to = (struct sockaddr_in6 *)&whereto;
	to->sin6_family = AF_INET6;
	to->sin6_port = htons(port);

	struct hostent *hp;
	if (inet_pton(AF_INET6, argv[1], &to->sin6_addr) > 0)
		hostname = argv[1];
	else if ((hp = gethostbyname2(argv[1], AF_INET6)))
	{
		memmove((caddr_t)&to->sin6_addr, hp->h_addr, sizeof(to->sin6_addr));
		hostname = (char *)hp->h_name;
	}
	else
		exit(1);

	int on = 1;
	setsockopt(icmp_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));

	sndsock = socket(AF_INET6, SOCK_DGRAM, 0);

	struct sockaddr_in6 firsthop = *to;
	firsthop.sin6_port = htons(1025);

	int probe_fd = socket(AF_INET6, SOCK_DGRAM, 0);
	connect(probe_fd, (struct sockaddr *)&firsthop, sizeof(firsthop));

	socklen_t alen = sizeof(saddr);
	getsockname(probe_fd, (struct sockaddr *)&saddr, &alen);

	close(probe_fd);
	saddr.sin6_port = 0;

	bind(sndsock, (struct sockaddr *)&saddr, sizeof(saddr));
	bind(icmp_sock, (struct sockaddr *)&saddr, sizeof(saddr));

	char pa[MAX_HOSTNAMELEN];
	fprintf(stderr, "traceroute to %s (%s)", hostname, inet_ntop(AF_INET6, &to->sin6_addr, pa, sizeof(pa)));
	fprintf(stderr, " from %s", inet_ntop(AF_INET6, &saddr.sin6_addr, pa, sizeof(pa)));
	fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, datalen);

	int seq = 0, tos = 0;
	for (int ttl = 1; ttl <= max_ttl; ++ttl)
	{
		printf("%2d ", ttl);

		struct in6_addr lastaddr = {0};

		int got_there = 0;
		int unreachable = 0;

		for (int probe = 0; probe < nprobes; ++probe)
		{
			send_probe(++seq, ttl);

			struct timeval t1, t2;
			gettimeofday(&t1, NULL);

			int cc;
			struct in6_addr to;
			while ((cc = wait_for_reply(icmp_sock, &from, &to, 1)) != 0)
			{
				gettimeofday(&t2, NULL);

				int code;
				if ((code = packet_ok(packet, cc, &from, &to, seq, &t1)))
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
						Printf(" !N");
						break;
					case ICMP6_DST_UNREACH_ADDR:
						++unreachable;
						Printf(" !H");
						break;
					case ICMP6_DST_UNREACH_ADMIN:
						++unreachable;
						Printf(" !S");
						break;
					}
					break;
				}
			}
			if (cc <= 0)
				Printf(" *");
			(void)fflush(stdout);
		}
		putchar('\n');
		if (got_there || (unreachable > 0 && unreachable >= nprobes - 1))
			exit(0);
	}
	return 0;
}

int wait_for_reply(int sock, struct sockaddr_in6 *from, struct in6_addr *to, int reset_timer)
{
	static struct timeval wait = {0, 0};
	if (reset_timer)
		wait.tv_sec = waittime;

	fd_set fds = {0};
	FD_SET(sock, &fds);
	int cc = 0;
	char cbuf[512];
	if (select(sock + 1, &fds, (fd_set *)0, (fd_set *)0, &wait) > 0)
	{
		struct iovec iov = {packet, sizeof(packet)};
		struct msghdr msg = {from, sizeof(*from), &iov, 1, cbuf, sizeof(cbuf), 0};
		cc = recvmsg(icmp_sock, &msg, 0);
		if (cc >= 0)
		{
			for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
			{
				if (cmsg->cmsg_level != SOL_IPV6)
					continue;
				switch (cmsg->cmsg_type)
				{
				case IPV6_PKTINFO:
					struct in6_pktinfo *ipi = (struct in6_pktinfo *)CMSG_DATA(cmsg);
					memcpy(to, ipi, sizeof(*to));
				}
			}
		}
	}
	return (cc);
}

void send_probe(int seq, int ttl)
{
	struct pkt_format pkt = {.ident = htonl(getpid()), .seq = htonl(seq)};
	gettimeofday(&pkt.tv, NULL);

	setsockopt(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
	sendto(sndsock, &pkt, datalen, 0, (struct sockaddr *)&whereto, sizeof(whereto));
}

double deltaT(struct timeval *t1p, struct timeval *t2p)
{
	return (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 + (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
}

int packet_ok(u_char *buf, int cc, struct sockaddr_in6 *from, struct in6_addr *to, int seq, struct timeval *tv)
{
	struct icmp6_hdr *icp = (struct icmp6_hdr *)buf;
	u_char type = icp->icmp6_type, code = icp->icmp6_code;
	if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT) || type == ICMP6_DST_UNREACH)
	{
		struct ip6_hdr *hip = (struct ip6_hdr *)(icp + 1);
		struct udphdr *up = (struct udphdr *)(hip + 1);
		int nexthdr = hip->ip6_nxt;
		if (nexthdr == IPPROTO_FRAGMENT)
			nexthdr = *(unsigned char *)up++;
		if (nexthdr == IPPROTO_UDP)
		{
			struct pkt_format *pkt = (struct pkt_format *)(up + 1);
			if (ntohl(pkt->ident) == ident && ntohl(pkt->seq) == seq)
			{
				*tv = pkt->tv;
				return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
			}
		}
	}
	return 0;
}

void print(struct sockaddr_in6 *from)
{
	char ip[MAX_HOSTNAMELEN];
	inet_ntop(AF_INET6, &from->sin6_addr, ip, sizeof(ip));

	struct hostent *hp = gethostbyaddr(&from->sin6_addr, sizeof(from->sin6_addr), AF_INET6);
	printf(" %s (%s)", hp ? hp->h_name : ip, ip);
}

void usage()
{
	fprintf(stderr, "Usage: traceroute host\n");
	exit(1);
}