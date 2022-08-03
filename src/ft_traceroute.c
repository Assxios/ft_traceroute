#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

#include "functions.h"

t_data g_data = {.options = {.family = AF_INET, .first_ttl = 1, .max_ttl = 30, .nprobes = 3}};

int main(int argc, char **argv)
{
	(void)argc;
	g_data.cmd = argv[0];

	resolve_flags(argv);
	generate_socket();

	g_data.sequence = (g_data.options.port ? g_data.options.port : (g_data.options.icmp ? 1 : 33434)) - 1;

	char buffer[USHRT_MAX] = {0};
	for (size_t i = 0; i < sizeof(buffer); i++)
		buffer[i] = 66;

	if (g_data.options.icmp)
	{
		struct icmphdr *icmp = (struct icmphdr *)buffer;
		bzero(icmp, sizeof(*icmp));

		icmp->type = g_data.server_addr.sa.sa_family == AF_INET ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
		icmp->un.echo.id = htons(getpid());
	}

	unsigned char got_reply = 0;
	for (unsigned int ttl = g_data.options.first_ttl; ttl <= g_data.options.max_ttl && !got_reply; ttl++)
	{
		update_ttl(ttl);
		printf("%2d ", ttl);

		struct sockaddr_storage from = {0};
		for (unsigned char probe = 0; probe < g_data.options.nprobes; probe++)
		{
			++g_data.sequence;
			if (!g_data.options.icmp)
			{
				g_data.server_addr.sa.sa_family == AF_INET ? (g_data.server_addr.sin.sin_port = htons(g_data.sequence)) : (g_data.server_addr.sin6.sin6_port = htons(g_data.sequence));
				sendto(g_data.send_sock, &buffer, g_data.options.datalen, 0, &g_data.server_addr.sa, g_data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
			}
			else
			{
				struct icmphdr *icmp = (struct icmphdr *)buffer;
				icmp->un.echo.sequence = htons(g_data.sequence);
				icmp->checksum = 0;
				icmp->checksum = checksum((unsigned short *)buffer, sizeof(*icmp) + g_data.options.datalen);
				sendto(g_data.recv_sock, buffer, sizeof(icmp) + g_data.options.datalen, 0, &g_data.server_addr.sa, g_data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
			}

			struct timeval time;
			gettimeofday(&time, NULL);

			int code = -1;
			while (code < 0)
			{
				code = recv_packet(&from, time);
				if (code == 1)
					got_reply++;
			}
		}
		printf("\n");
	}
}
