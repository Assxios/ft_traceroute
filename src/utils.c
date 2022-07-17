#include <stdio.h>
#include <stdlib.h>

#include "types.h"

void error(char *msg, char *detail)
{
	printf("%s: %s: %s\n", g_data.cmd, msg, detail);
	exit(1);
}
