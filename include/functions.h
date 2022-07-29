#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "types.h"

// parse
void resolve_flags(char **argv);

// icmp
int resolve_addr(char *host);
void generate_socket();
void update_ttl(unsigned int ttl);

// network
int recv_packet(struct sockaddr_storage *from, struct timeval last);

// utils
void error(char *msg, char *detail);
int is_digit(char *str);
unsigned short checksum(unsigned short *address, size_t len);

#endif
