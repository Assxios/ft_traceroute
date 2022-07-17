#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "types.h"

void resolve_flags(char **argv);

int resolve_addr(char *host);
void generate_socket();
void update_ttl(unsigned int ttl);

int recv_packet(struct sockaddr_storage *from, struct timeval last);

void error(char *msg, char *detail);
#endif
