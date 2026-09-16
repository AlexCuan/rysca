#ifndef _RIPV2_ROUTE_TABLE_H
#define _RIPV2_ROUTE_TABLE_H

#include <time.h> // Needed for time_t
#include "../ipv4/ipv4.h"

typedef struct ripv2_route
{
  ipv4_addr_t subnet; // Called 'subnet_addr' in the original IPv4 table
  ipv4_addr_t mask; // Called 'subnet_mask' in the original IPv4 table
  ipv4_addr_t next_hop; // Called 'gateway_addr' in the original IPv4 table
  uint32_t metric; // RIP metric
  uint16_t route_tag; // Route tag (RFC 2453)
  time_t last_updated; // Used to track the timeout (180s)
  int is_garbage; // Garbage collection flag
  int is_static; // Route read from file: not expired by the timers
} ripv2_route_t;


ripv2_route_t* ripv2_route_create
(ipv4_addr_t subnet, ipv4_addr_t mask, ipv4_addr_t next_hop, uint32_t metric);

void ripv2_route_print(ripv2_route_t* route);

void ripv2_route_free(ripv2_route_t* route);

#define ripv2_ROUTE_TABLE_SIZE 256

typedef struct ripv2_route_table ripv2_route_table_t;

ripv2_route_table_t* ripv2_route_table_create();

int ripv2_route_table_add(ripv2_route_table_t* table, ripv2_route_t* route);

ripv2_route_t* ripv2_route_table_remove(ripv2_route_table_t* table, int index);

ripv2_route_t* ripv2_route_table_lookup(ripv2_route_table_t* table, ipv4_addr_t subnet, ipv4_addr_t mask);

ripv2_route_t* ripv2_route_table_get(ripv2_route_table_t* table, int index);

int ripv2_route_table_find
(ripv2_route_table_t* table, ipv4_addr_t subnet, ipv4_addr_t mask);

void ripv2_route_table_free(ripv2_route_table_t* table);

int ripv2_route_table_read(char* filename, ripv2_route_table_t* table);

void ripv2_route_table_print(ripv2_route_table_t* table);

int ripv2_route_table_write(ripv2_route_table_t* table, char* filename);

int ripv2_route_table_size(ripv2_route_table_t* table);


#endif /* _IPv4_ROUTE_TABLE_H */
