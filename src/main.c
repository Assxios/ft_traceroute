#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>

#include "functions.h"

t_data g_data = {.port = 33434, .options = {.max_ttl = 30, .nprobes = 3, .family = AF_INET}};

int main(int argc, char **argv)
{
	(void)argc;
	g_data.cmd = argv[0];

	if (getuid() != 0)
		error("usage error", "You must be root to run this program");

	resolve_flags(argv);
	generate_socket();

	for (unsigned int ttl = 1; ttl <= g_data.options.max_ttl; ttl++)
	{
		update_ttl(ttl);
		printf("%2d ", ttl);

		unsigned char got_reply = 0;
		struct sockaddr_storage from = {0};

		for (unsigned char probe = 0; probe < g_data.options.nprobes; probe++)
		{
			if (g_data.server_addr.sa.sa_family == AF_INET)
				g_data.server_addr.sin.sin_port = htons(++g_data.port);
			else
				g_data.server_addr.sin6.sin6_port = htons(++g_data.port);

			sendto(g_data.send_sock, NULL, 0, 0, &g_data.server_addr.sa, g_data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));

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

		if (got_reply)
			break;
	}
}