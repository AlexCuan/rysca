#ifndef IPV4_SERVER_H
#define IPV4_SERVER_H

#include "ipv4.h"

int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol, unsigned char buffer [], ipv4_addr_t sender, int buf_len, long int timeout);
ipv4_layer_t* ipv4_open(char * file_conf, char * file_conf_route);

#endif //IPV4_SERVER_H
