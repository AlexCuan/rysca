#ifndef _IPv4_H
#define _IPv4_H

#include <stdint.h>
#include "../eth/eth.h"
#include "ipv4_route_table.h"

#define IPv4_ADDR_SIZE 4
#define IPv4_STR_MAX_LENGTH 16

typedef unsigned char ipv4_addr_t[IPv4_ADDR_SIZE];

typedef struct ipv4_layer
{
    eth_iface_t* iface; /* Network interface this layer sits on */
    ipv4_addr_t addr; /* IP address of this layer */
    ipv4_addr_t netmask; /* Network mask of this layer */
    ipv4_route_table_t* routing_table; /* Route table */
} ipv4_layer_t;

typedef struct ipv4_header
{
    uint8_t version_ihl;
    uint8_t type_of_service;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t time_to_live;
    uint8_t protocol;
    uint16_t header_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dest_addr;
} ipv4_header_t;

/* The structures in this stack are written straight onto the wire. Today the
   ABI lays them out with no gaps, but that is not guaranteed: failing to
   compile is better than emitting malformed packets. */
_Static_assert(sizeof(ipv4_header_t) == 20, "padded IPv4 header");

/* All-zero IPv4 address "0.0.0.0" */
extern ipv4_addr_t IPv4_ZERO_ADDR;

/* Maximum length of a network interface name */
#define IFACE_NAME_MAX_LENGTH 32


/* void ipv4_addr_str ( ipv4_addr_t addr, char* str );
 *
 * DESCRIPTION:
 * This function generates a string representing the given IPv4 address.
 *
 * PARAMETERS:
 * 'addr': The IP address to represent as text.
 * 'str': Memory where the generated string is to be stored.
 * At least 'IPv4_STR_MAX_LENGTH' bytes must be reserved.
 */
void ipv4_addr_str(ipv4_addr_t addr, char* str);


/* int ipv4_str_addr ( char* str, ipv4_addr_t addr );
 *
 * DESCRIPTION:
 * This function scans a string looking for an IPv4 address.
 *
 * PARAMETERS:
 * 'str': The string to process.
 * 'addr': Memory where the IPv4 address found is stored.
 *
 * RETURN VALUE:
 * Returns 0 if the string represented an IPv4 address.
 *
 * ERRORS:
 * The function returns -1 if the string did not represent an IPv4 address.
 */
int ipv4_str_addr(char* str, ipv4_addr_t addr);


/*
 * uint16_t ipv4_checksum ( unsigned char * data, int len )
 *
 * DESCRIPTION:
 * This function computes the IP checksum of the given data.
 *
 * PARAMETERS:
 * 'data': Pointer to the data the checksum is computed over.
 * 'len': Length in bytes of the data.
 *
 * RETURN VALUE:
 * The value of the computed checksum.
 */
uint16_t ipv4_checksum(unsigned char* data, int len);

/*
 * ipv4_send: Added 'corrupt' parameter.
 * If corrupt is 1, the IPv4 header checksum will be intentionally corrupted.
 */
int ipv4_send(ipv4_layer_t* layer, ipv4_addr_t dst, uint8_t protocol, unsigned char* payload, int payload_len,
              int corrupt);

/* Returns the number of payload bytes received, 0 if the timer expired
   or -1 if an error occurred. */
int ipv4_recv(ipv4_layer_t* layer, uint8_t protocol, unsigned char buffer[], ipv4_addr_t sender, ipv4_addr_t dest,
              int buf_len, long int timeout);
int ipv4_close(ipv4_layer_t* layer);

ipv4_layer_t* ipv4_open(char* file_conf, char* file_conf_route);

#endif /* _IPv4_H */
