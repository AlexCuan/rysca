//
// Created by alex on 5/12/25.
//

#ifndef TCP_IP_STACK_RIPV2_ROUTE_TABLE_H
#define TCP_IP_STACK_RIPV2_ROUTE_TABLE_H
#include "../ipv4/ipv4.h"
#include <time.h>

typedef struct ripv2_route {
    ipv4_addr_t subnet;
    ipv4_addr_t mask;
    ipv4_addr_t next_hop; // Dirección del router que nos anunció la ruta
    uint32_t metric;
    uint16_t route_tag;
    time_t last_updated;  // Timestamp para controlar el timeout (180s)
    int is_garbage;       // Flag para garbage collection (opcional para básico, recomendado)
} ripv2_route_t;

typedef struct ripv2_route_table ripv2_route_table_t;

ripv2_route_table_t *ripv2_route_table_create();
void ripv2_route_table_free(ripv2_route_table_t *table);
int ripv2_route_table_add(ripv2_route_table_t *table, ripv2_route_t *route);
ripv2_route_t *ripv2_route_table_lookup(ripv2_route_table_t *table, ipv4_addr_t subnet, ipv4_addr_t mask);
ripv2_route_t *ripv2_route_table_get(ripv2_route_table_t *table, int index);
int ripv2_route_table_size(ripv2_route_table_t *table);
void ripv2_route_table_print(ripv2_route_table_t *table);
#endif //TCP_IP_STACK_RIPV2_ROUTE_TABLE_H