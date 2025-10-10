#ifndef UDP_H
#define UDP_H

#include "../ipv4/ipv4.h"
#include <stdint.h>

#define IP_PROTOCOL_UDP 17

typedef struct {
    ipv4_layer_t* ipv4_layer;
} udp_layer_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

udp_layer_t* udp_open(char* config_file, char* route_table);
int udp_close(udp_layer_t* layer);
int udp_send(udp_layer_t* layer, ipv4_addr_t dest_addr, uint16_t dest_port, unsigned char* payload, int payload_len);
int udp_rcv(udp_layer_t* layer, uint16_t* src_port, ipv4_addr_t src_addr, unsigned char* buffer, int buffer_len, long int timeout);

#endif //UDP_H
