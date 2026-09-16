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

_Static_assert(sizeof(ripv2_entry_t) == RIP_ENTRY_SIZE, "entrada RIPv2 con relleno");
_Static_assert(sizeof(ripv2_msg_t) == RIP_HEADER_SIZE + RIP_MAX_ENTRIES * RIP_ENTRY_SIZE,
               "mensaje RIPv2 con relleno");

// Forward declaration para no incluir el .h de la tabla y causar ciclos
struct ripv2_route_table;

/* Procesa un Response RIPv2. 'msg_len' son los bytes realmente recibidos: sin
   el las entradas no escritas por el emisor se leerian del contenido anterior
   del buffer. Devuelve 1 si la tabla cambio. */
int ripv2_process_response(struct ripv2_route_table* table, ripv2_msg_t* msg, int msg_len, ipv4_addr_t src_ip);

#endif //TCP_IP_STACK_RIPV2_H
