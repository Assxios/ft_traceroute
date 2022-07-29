#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>

#include "functions.h"

void print_help()
{
	// usage
	printf("Usage\n  %s [ -46dn ] [ -f first_ttl ] [ -m max_ttl ] [ -p port ] [ -q nqueries ] host\n\n", g_data.cmd);

	// options
	printf("Options:\n");
	printf("  -4                          use IPv4\n");
	printf("  -6                          use IPv6\n");
	printf("  -d                          Enable socket level debugging\n");
	printf("  -f first_ttl                Start from the first_ttl hop (instead from 1)\n");
	printf("  -I  --icmp                  Use ICMP ECHO for tracerouting\n");
	printf("  -m max_ttl                  Set the max number of hops (max TTL to be reached). Default is 30\n");
	printf("  -n                          Do not resolve IP addresses to their domain names\n");
	//printf("  -p port                     initial seq for "icmp" (incremented as well, default from 1)\n");
	printf("  -q nqueries                 Set the number of probes per each hop. Default is 3\n");
	printf("  --help                      Read this help and exit\n\n");
	
	// arguments
	printf("Arguments:\n");
	printf("+     host          The host to traceroute to\n\n");

	//bg
	printf("Made with ♥ by hallainea and Assxios\n");
	exit(0);
}

size_t get_number(char ***av, size_t min, size_t max)
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

	if (nbr < min || nbr > max)
		error("usage error", "Value out of range");
	return nbr;
}

void options_long(char ***av)
{
	if (strcmp(**av, "-help") == 0)
		print_help();
	if (strcmp(**av, "--icmp") == 0)
		g_data.options.icmp = true;
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
		case 'd':
			g_data.options.debug = true;
			break;
		case 'f':
			g_data.options.first_ttl = get_number(av, 1, IPDEFTTL);
			return;
		case 'I':
			g_data.options.icmp = true;
			break;
		case '-':
			options_long(av);
		default:
			sprintf(buffer, "Invalid option -%s", **av);
			error("usage error", buffer);
		}
}

void resolve_flags(char **argv)
{
	if (getuid() != 0)
		error("usage error", "You must be root to run this program");
	if (argv[1] == NULL)
		print_help();

	while (*++argv && **argv == '-')
		options(&argv);

	if (!*argv)
		error("usage error", "Missing host");
	if (resolve_addr(*argv) != 0)
		error("usage error", "Temporary failure in name resolution");
	if (*(argv + 1))
		error("usage error", "Extranous argument found");

	char ip_str[INET6_ADDRSTRLEN];
	g_data.server_addr.sa.sa_family == AF_INET ? inet_ntop(AF_INET, &g_data.server_addr.sin.sin_addr, ip_str, INET_ADDRSTRLEN) : inet_ntop(AF_INET6, &g_data.server_addr.sin6.sin6_addr, ip_str, INET6_ADDRSTRLEN);
	printf("traceroute to %s (%s), %d hops max, %d byte packets\n", *argv, ip_str, g_data.options.max_ttl, 0);

}
