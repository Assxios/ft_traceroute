#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>

struct in6_pktinfo
{
	struct in6_addr ipi6_addr; /* src/dst IPv6 address */
	unsigned int ipi6_ifindex; /* send/recv interface index */
};

unsigned short checksum(unsigned short *address, size_t len)
{
	unsigned short sum = 0;
	while (len -= sizeof(short))
		sum += *address++;
	return (~sum);
}

void *ancillary_data(struct msghdr msg, int len, int level)
{
	for (struct cmsghdr *cmsgptr = CMSG_FIRSTHDR(&msg);
		 cmsgptr != NULL;
		 cmsgptr = CMSG_NXTHDR(&msg, cmsgptr))
	{
		if (cmsgptr->cmsg_len == 0)
			break;

		if (cmsgptr->cmsg_level == len && cmsgptr->cmsg_type == level)
			return CMSG_DATA(cmsgptr);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: %s <host>\n", argv[0]);
		return (EXIT_FAILURE);
	}

	struct addrinfo *result, hints = {0};

	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	int s = getaddrinfo(argv[1], NULL, &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	int sfd = socket(result->ai_family, result->ai_socktype, IPPROTO_ICMPV6);
	if (sfd == -1)
	{
		fprintf(stderr, "socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	int on = 1;
	if (setsockopt(sfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) < 0)
	{
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	int hops = 2;
	if (setsockopt(sfd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(hops)) < 0)
	{
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct icmphdr icmp_hdr = {0};
	icmp_hdr.type = ICMP6_ECHO_REQUEST;
	icmp_hdr.un.echo.id = getpid();
	icmp_hdr.un.echo.sequence = 3;
	icmp_hdr.checksum = checksum((void *)&icmp_hdr, sizeof(struct icmphdr));

	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)result->ai_addr;
	s = sendto(sfd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr *)addr, sizeof(*addr));
	if (s == -1)
	{
		fprintf(stderr, "sendto: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Sent %d bytes\n", s);

	struct icmphdr recv_hdr;
	struct iovec iov = {&recv_hdr, sizeof(recv_hdr)};
	struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1};
	char data[64] = {0};
	msg.msg_control = data;
	msg.msg_controllen = sizeof(data);

	if (recvmsg(sfd, &msg, 0) == -1)
	{
		fprintf(stderr, "recvmsg: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Received %ld bytes\n", msg.msg_iovlen);
	struct in6_pktinfo *pktinfo = ancillary_data(msg, IPPROTO_IPV6, IPV6_PKTINFO);
	if (pktinfo == NULL)
	{
		fprintf(stderr, "No pktinfo\n");
		exit(EXIT_FAILURE);
	}
	char ip6_str[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &pktinfo->ipi6_addr, ip6_str, sizeof(ip6_str));
	printf("Received from %s\n", ip6_str);

	close(sfd);
	freeaddrinfo(result);
	return 0;
}
