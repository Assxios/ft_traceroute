#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/time.h>

struct sockaddr server_addr = {0};
unsigned int max_ttl = 30;
unsigned char nprobes = 3;

int get_addr(char *host, struct sockaddr *addr)
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

struct pkt_format
{
	unsigned int id;
	unsigned int seq;
};

int main(int argc, char **argv)
{
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

	if (get_addr(argv[1], &server_addr) != 0)
	{
		printf("Could not resolve hostname.\n");
		return (1);
	}

	char ip_str[INET6_ADDRSTRLEN];
	if (server_addr.sa_family == AF_INET)
		inet_ntop(AF_INET, &((struct sockaddr_in *)&server_addr)->sin_addr, ip_str, INET_ADDRSTRLEN);
	else
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&server_addr)->sin6_addr, ip_str, INET6_ADDRSTRLEN);
	printf("traceroute to %s (%s), %d hops max, %ld byte packets\n", argv[1], ip_str, max_ttl, sizeof(struct pkt_format));

	if (server_addr.sa_family == AF_INET)
		((struct sockaddr_in *)&server_addr)->sin_port = htons(33434);
	else
		((struct sockaddr_in6 *)&server_addr)->sin6_port = htons(33434);
	// port 33434 is used by mtr (traceroute)

	int send_sock = socket(server_addr.sa_family, SOCK_DGRAM, 0);
	int recv_sock = socket(server_addr.sa_family, SOCK_RAW, server_addr.sa_family == AF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6);
	if (send_sock < 0 || recv_sock < 0)
	{
		printf("Could not create socket.\n");
		return (1);
	}

	struct timeval tv = {1, 0};
	setsockopt(recv_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	int on = 1;
	setsockopt(recv_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));

	int got_reply = 0, seq = 0;
	for (unsigned int ttl = 1; ttl <= max_ttl && !got_reply; ttl++)
	{
		if (server_addr.sa_family == AF_INET)
			setsockopt(send_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
		else
			setsockopt(send_sock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
		printf("%d ", ttl);

		struct sockaddr_storage from = {0};

		for (unsigned char probe = 0; probe < nprobes; probe++)
		{
			struct pkt_format pkt = {.id = getpid(), .seq = ++seq};
			if (sendto(send_sock, &pkt, sizeof(pkt), 0, &server_addr, server_addr.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) < 0)
			{
				printf("Could not send packet.\n");
				return (1);
			}

			struct timeval start, end;
			gettimeofday(&start, NULL);

			struct sockaddr_storage addr = {0};

			char packet[512];
			char ctrl[512];
			struct iovec iov = {.iov_base = packet, .iov_len = sizeof(packet)};
			struct msghdr msg = {.msg_name = &addr, .msg_namelen = sizeof(addr), .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctrl, .msg_controllen = sizeof(ctrl)};

			if (recvmsg(recv_sock, &msg, 0) < 0)
			{
				printf(" *");
				continue;
			}
			gettimeofday(&end, NULL);

			if (server_addr.sa_family == AF_INET)
				memcpy(&addr, &((struct ip *)packet)->ip_src, sizeof(struct in_addr));
			else
				for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
					if (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
						memcpy(&addr, CMSG_DATA(cmsg), sizeof(addr));

			if (memcmp(&addr, &from, sizeof(addr)) != 0)
			{
				char ip_str[INET6_ADDRSTRLEN];
				if (server_addr.sa_family == AF_INET)
					inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr, ip_str, INET_ADDRSTRLEN);
				else
					inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr, ip_str, INET6_ADDRSTRLEN);

				struct hostent *host = gethostbyaddr(&addr, server_addr.sa_family == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr), server_addr.sa_family);
				printf(" %s (%s)", host ? host->h_name : ip_str, ip_str);

				memcpy(&from, &addr, sizeof(from));
			}
			printf(" %.3f ms", (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0);
		}
		printf("\n");
	}
}