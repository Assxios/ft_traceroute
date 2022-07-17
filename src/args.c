#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "functions.h"

void usage(void)
{
	printf("usage: %s [-f family] [-p port] [-t max_ttl] [-n nprobes]\n", g_data.cmd);
	exit(1);
}

int is_digit(char *str)
{
	while ((*str >= '0' && *str <= '9'))
		str++;
	return !*str;
}

size_t get_number(char ***av, size_t max)
{
	if (*(**av + 1) != '\0')
	{
		char buffer[1 << 10];
		sprintf(buffer, "Invalid option -%s", **av);
		error("usage error", buffer);
	}

	size_t nbr = 0;
	if (*++*av && is_digit(**av))
		while (***av >= '0' && ***av <= '9')
			nbr = nbr * 10 + *(**av)++ - '0';

	if (nbr == 0 || nbr > max)
		error("usage error", "Value out of range");
	return nbr;
}

void options(char ***av)
{
	char buffer[1 << 10];

	while (*++**av)
		switch (***av)
		{
		case '4':
			g_data.options.family = AF_INET;
			break;
		case '6':
			g_data.options.family = AF_INET6;
			break;
		case '-':
			if (strcmp(**av, "-help") == 0)
				usage();
		default:
			sprintf(buffer, "Invalid option -%s", **av);
			error("usage error", buffer);
		}
}

void resolve_flags(char **argv)
{
	if (argv[1] == NULL)
		usage();
	while (*++argv && **argv == '-')
		options(&argv);

	if (!*argv)
		error("usage error", "Missing host");
	if (resolve_addr(*argv) != 0)
		error("usage error", "Temporary failure in name resolution");

	char ip_str[INET6_ADDRSTRLEN];
	if (g_data.server_addr.sa.sa_family == AF_INET)
		inet_ntop(AF_INET, &g_data.server_addr.sin.sin_addr, ip_str, INET_ADDRSTRLEN);
	else
		inet_ntop(AF_INET6, &g_data.server_addr.sin6.sin6_addr, ip_str, INET6_ADDRSTRLEN);
	printf("traceroute to %s (%s), %d hops max, %d byte packets\n", *argv, ip_str, g_data.options.max_ttl, 0);

	if (*++argv)
		error("usage error", "Extranous argument found");
}
