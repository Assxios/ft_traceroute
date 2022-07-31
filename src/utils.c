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

unsigned short checksum(unsigned short *address, size_t len)
{
	unsigned short sum = 0;
	while (len -= sizeof(short))
		sum += *address++;
	return (~sum);
}
