#include <stdio.h>
#include <stdlib.h>

#include "types.h"

void error(char *msg, char *detail)
{
	printf("%s: %s: %s\n", g_data.cmd, msg, detail);
	exit(1);
}

int is_digit(char *str)
{
	while ((*str >= '0' && *str <= '9'))
		str++;
	return !*str;
}

unsigned short checksum(unsigned short *addr, size_t len)
{
	unsigned int sum = 0;
	for (; len > sizeof(char); len -= sizeof(short))
		sum += *addr++;
	if (len == sizeof(char))
		sum += *(unsigned char *)addr;
	while (sum >> sizeof(short) * 8)
		sum = (sum & 0xffff) + (sum >> sizeof(short) * 8);
	return (~sum);
}
