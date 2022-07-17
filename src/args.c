#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include "types.h"
#include "functions.h"

void check_args(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: %s <host>\n", argv[0]);
		exit(1);
	}

	if (getuid() != 0)
	{
		printf("You must be root to run this program.\n");
		exit(2);
	}
}

void resolve_flags(char **argv, struct sockaddr_storage *server_addr, t_options *options)
{
	if (resolve_addr(argv[1], server_addr) != 0)
	{
		printf("Could not resolve hostname.\n");
		exit(3);
	}

	char ip_str[INET6_ADDRSTRLEN];
	if (((struct sockaddr *)server_addr)->sa_family == AF_INET)
		inet_ntop(AF_INET, &((struct sockaddr_in *)server_addr)->sin_addr, ip_str, INET_ADDRSTRLEN);
	else
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)server_addr)->sin6_addr, ip_str, INET6_ADDRSTRLEN);
	printf("traceroute to %s (%s), %d hops max, %d byte packets\n", argv[1], ip_str, options->max_ttl, 0);
}
