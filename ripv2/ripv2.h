#ifndef TCP_IP_STACK_RIPV2_H
#define TCP_IP_STACK_RIPV2_H

#include <stdint.h>
#include "../ipv4/ipv4.h"

#define RIP_PORT 520
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2
#define RIP_VERSION 2
#define RIP_MAX_ENTRIES 25
#define RIP_HEADER_SIZE 4
#define RIP_ENTRY_SIZE 20

typedef struct
{
    uint16_t family;
    uint16_t tag;
    ipv4_addr_t ip;
    ipv4_addr_t mask;
    ipv4_addr_t next_hop;
    uint32_t metric;
} ripv2_entry_t;

typedef struct
{
    uint8_t command;
    uint8_t version;
    uint16_t zero;
    ripv2_entry_t entries[RIP_MAX_ENTRIES];
} ripv2_msg_t;

_Static_assert(sizeof(ripv2_entry_t) == RIP_ENTRY_SIZE, "padded RIPv2 entry");
_Static_assert(sizeof(ripv2_msg_t) == RIP_HEADER_SIZE + RIP_MAX_ENTRIES * RIP_ENTRY_SIZE,
               "padded RIPv2 message");

// Forward declaration to avoid including the table header and causing cycles
struct ripv2_route_table;

/* Processes a RIPv2 Response. 'msg_len' is the number of bytes actually
   received: without it, the entries the sender did not write would be read from
   the previous contents of the buffer. Returns 1 if the table changed. */
int ripv2_process_response(struct ripv2_route_table* table, ripv2_msg_t* msg, int msg_len, ipv4_addr_t src_ip);

#endif //TCP_IP_STACK_RIPV2_H
