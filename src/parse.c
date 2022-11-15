#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <limits.h>

#include "functions.h"

void print_help()
{
	// usage
	printf("Usage\n  %s [ -46dIn ] [ -f first_ttl ] [ -m max_ttl ] [ -p port ] [ -q nqueries ] host [ packetlen ]\n\n", g_data.cmd);

	// options
	printf("Options:\n");
	printf("  -4                          Use IPv4\n");
	printf("  -6                          Use IPv6\n");
	printf("  -d                          Enable socket level debugging\n");
	printf("  -f first_ttl                Start from the first_ttl hop (instead from 1)\n");
	printf("  -I                          Use ICMP ECHO for tracerouting\n");
	printf("  -m max_ttl                  Set the max number of hops (max TTL to be reached). Default is 30\n");
	printf("  -n                          Do not resolve IP addresses to their domain names\n");
	printf("  -p port                     Set the destination port to use. It is either initial udp port value for \"default\" \n \
                             method (incremented by each probe, default is 33434), or initial seq for \"icmp\"\n \
                             (incremented as well, default from 1)\n");
	printf("  -q nqueries                 Set the number of probes per each hop. Default is 3\n");
	printf("  --help                      Read this help and exit\n\n");

	// arguments
	printf("Arguments:\n");
	printf("+     host          The host to traceroute to\n");
	printf("      packetlen     The full packet length (default is the length of an header plus 40). Can be ignored or\n \
	            increased to a minimal allowed value\n\n");

	// bg
	printf("Made with ♥ by execrate0 and Assxios\n");
	exit(0);
}

size_t get_number(char ***av, size_t min, size_t max, bool option_check)
{
	char buffer[1 << 10];

	if (option_check && *(**av + 1) != '\0')
	{
		sprintf(buffer, "Invalid option -%s", **av);
		error("usage error", buffer);
	}

	if (!is_digit(*++*av))
	{
		sprintf(buffer, "Invalid argument '%s'", **av);
		error("usage error", buffer);
	}

	size_t nbr = 0;
	while (***av >= '0' && ***av <= '9')
		nbr = nbr * 10 + *(**av)++ - '0';

	if (nbr < min || nbr > max)
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
		case 'd':
			g_data.options.debug = true;
			break;
		case 'f':
			g_data.options.first_ttl = get_number(av, 1, g_data.options.max_ttl, true);
			return;
		case 'I':
			g_data.options.icmp = true;
			break;
		case 'm':
			g_data.options.max_ttl = get_number(av, 1, UCHAR_MAX, true);
			return;
		case 'n':
			g_data.options.resolve = false;
			break;
		case 'p':
			g_data.options.port = get_number(av, 1, USHRT_MAX, true);
			return;
		case 'q':
			g_data.options.nprobes = get_number(av, 1, 10, true);
			return;
		case '-':
			if (strcmp(**av, "-help") == 0)
				print_help();
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
	{
		if (*(argv + 2))
			error("usage error", "Extranous argument found");

		g_data.options.datalen = get_number(&argv, 0, USHRT_MAX, false);
		argv--;

		if (g_data.options.datalen < DEFAULT_PACKET_SIZE)
			g_data.options.datalen = DEFAULT_PACKET_SIZE;
		g_data.options.datalen -= DEFAULT_PACKET_SIZE;
	}

	char ip_str[INET6_ADDRSTRLEN];
	g_data.server_addr.sa.sa_family == AF_INET ? inet_ntop(AF_INET, &g_data.server_addr.sin.sin_addr, ip_str, INET_ADDRSTRLEN) : inet_ntop(AF_INET6, &g_data.server_addr.sin6.sin6_addr, ip_str, INET6_ADDRSTRLEN);
	printf("traceroute to %s (%s), %d hops max, %d byte packets\n", *argv, ip_str, g_data.options.max_ttl, DEFAULT_PACKET_SIZE + g_data.options.datalen);
}
