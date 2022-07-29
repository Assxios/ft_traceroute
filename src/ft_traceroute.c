#include <stdio.h>
#include <sys/time.h>

#include "functions.h"

t_data g_data = {.port = 33434, .options = {.max_ttl = 30, .nprobes = 3, .family = AF_INET, .first_ttl = 1}};

int main(int argc, char **argv)
{
	(void)argc;
	g_data.cmd = argv[0];
	unsigned char got_reply = 0;

	resolve_flags(argv);
	generate_socket();

	if (g_data.options.icmp)
	{
		g_data.icmp.type = g_data.server_addr.sa.sa_family == AF_INET ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
		g_data.icmp.checksum = checksum((void *)&g_data.icmp, sizeof(struct icmphdr));
	}

	for (unsigned int ttl = g_data.options.first_ttl; ttl <= g_data.options.max_ttl && !got_reply; ttl++)
	{
		update_ttl(ttl);
		printf("%2d ", ttl);

		struct sockaddr_storage from = {0};
		char buffer[60];

		for (unsigned char probe = 0; probe < g_data.options.nprobes; probe++)
		{
			if (g_data.options.icmp == false)
			{
				g_data.server_addr.sa.sa_family == AF_INET ? (g_data.server_addr.sin.sin_port = htons(++g_data.port)) : (g_data.server_addr.sin6.sin6_port = htons(++g_data.port));
				sendto(g_data.send_sock, &buffer, sizeof(buffer), 0, &g_data.server_addr.sa, g_data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
			}
			else
			{
				sendto(g_data.recv_sock, &g_data.icmp, sizeof(g_data.icmp), 0, &g_data.server_addr.sa, g_data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
				g_data.icmp.un.echo.sequence++;
				g_data.icmp.checksum--;
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
