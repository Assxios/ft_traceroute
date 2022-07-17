#include <stdio.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "types.h"
#include "functions.h"

int main(int argc, char **argv)
{
	t_data data = {.port = 33434, .options = {.max_ttl = 30, .nprobes = 3}};

	check_args(argc, argv);
	resolve_flags(argv, &data.server_addr.ss, &data.options);
	generate_socket(data.server_addr.sa.sa_family, &data.send_sock, &data.recv_sock);

	for (unsigned int ttl = 1; ttl <= data.options.max_ttl; ttl++)
	{
		update_ttl(data.send_sock, data.server_addr.sa.sa_family, ttl);
		printf("%2d ", ttl);

		unsigned char got_reply = 0;
		struct sockaddr_storage from = {0};

		for (unsigned char probe = 0; probe < data.options.nprobes; probe++)
		{
			if (data.server_addr.sa.sa_family == AF_INET)
				((struct sockaddr_in *)&data.server_addr)->sin_port = htons(++data.port);
			else
				((struct sockaddr_in6 *)&data.server_addr)->sin6_port = htons(++data.port);

			sendto(data.send_sock, NULL, 0, 0, &data.server_addr.sa, data.server_addr.sa.sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));

			struct timeval time;
			gettimeofday(&time, NULL);

			int code = -1;
			while (code < 0)
			{
				code = recv_packet(data.recv_sock, data.server_addr.sa.sa_family, data.port, &from, time);
				if (code == 1)
					got_reply++;
			}
		}
		printf("\n");

		if (got_reply)
			break;
	}
}