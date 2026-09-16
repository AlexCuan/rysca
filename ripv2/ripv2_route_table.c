#include "ripv2_route_table.h"
#include "../ipv4/ipv4_route_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>

/* Opaque structure for the RIPv2 route table */
struct ripv2_route_table
{
  ripv2_route_t* routes[ripv2_ROUTE_TABLE_SIZE];
};

/* ripv2_route_t * ripv2_route_create * ( ipv4_addr_t subnet, ipv4_addr_t mask, ipv4_addr_t next_hop, uint32_t metric )
 * * DESCRIPTION:
 * Creates a RIPv2 route and initialises its timers.
 */
ripv2_route_t* ripv2_route_create
(ipv4_addr_t subnet, ipv4_addr_t mask, ipv4_addr_t next_hop, uint32_t metric)
{
  ripv2_route_t* route = (ripv2_route_t*)malloc(sizeof(ripv2_route_t));

  if (route != NULL)
  {
    memcpy(route->subnet, subnet, IPv4_ADDR_SIZE);
    memcpy(route->mask, mask, IPv4_ADDR_SIZE);
    memcpy(route->next_hop, next_hop, IPv4_ADDR_SIZE);
    route->metric = metric;
    route->route_tag = 0;
    route->last_updated = time(NULL); // Initialise with the current timestamp
    route->is_garbage = 0;
    route->is_static = 0;
  }

  return route;
}

/* void ripv2_route_free ( ripv2_route_t * route ) */
void ripv2_route_free(ripv2_route_t* route)
{
  if (route != NULL)
  {
    free(route);
  }
}

/* void ripv2_route_print ( ripv2_route_t * route )
 * Prints the state of the RIP route, including its timers.
 */
void ripv2_route_print(ripv2_route_t* route)
{
  if (route != NULL)
  {
    char subnet_str[IPv4_STR_MAX_LENGTH];
    char mask_str[IPv4_STR_MAX_LENGTH];
    char nh_str[IPv4_STR_MAX_LENGTH];

    ipv4_addr_str(route->subnet, subnet_str);
    ipv4_addr_str(route->mask, mask_str);
    ipv4_addr_str(route->next_hop, nh_str);

    // Seconds elapsed since the last update
    double seconds_since_update = difftime(time(NULL), route->last_updated);

    printf("%s/%s -> Nexthop: %s | Metric: %2d | Age: %3.0fs | Garbage: %s\n",
           subnet_str, mask_str, nh_str, route->metric,
           seconds_since_update,
           (route->is_garbage ? "YES" : "NO"));
  }
}

/* ripv2_route_table_t * ripv2_route_table_create() */
ripv2_route_table_t* ripv2_route_table_create()
{
  ripv2_route_table_t* table;

  table = (ripv2_route_table_t*)malloc(sizeof(struct ripv2_route_table));
  if (table != NULL)
  {
    int i;
    for (i = 0; i < ripv2_ROUTE_TABLE_SIZE; i++)
    {
      table->routes[i] = NULL;
    }
  }

  return table;
}

/* void ripv2_route_table_free ( ripv2_route_table_t * table ) */
void ripv2_route_table_free(ripv2_route_table_t* table)
{
  if (table != NULL)
  {
    int i;
    for (i = 0; i < ripv2_ROUTE_TABLE_SIZE; i++)
    {
      if (table->routes[i] != NULL)
      {
        ripv2_route_free(table->routes[i]);
        table->routes[i] = NULL;
      }
    }
    free(table);
  }
}

/* int ripv2_route_table_add ( ripv2_route_table_t * table, ripv2_route_t * route ) */
int ripv2_route_table_add(ripv2_route_table_t* table, ripv2_route_t* route)
{
  int route_index = -1;

  if (table != NULL && route != NULL)
  {
    int i;
    for (i = 0; i < ripv2_ROUTE_TABLE_SIZE; i++)
    {
      if (table->routes[i] == NULL)
      {
        table->routes[i] = route;
        route_index = i;
        break;
      }
    }
  }

  return route_index;
}

/* ripv2_route_t * ripv2_route_table_remove ( ripv2_route_table_t * table, int index ) */
ripv2_route_t* ripv2_route_table_remove(ripv2_route_table_t* table, int index)
{
  ripv2_route_t* removed_route = NULL;

  if ((table != NULL) && (index >= 0) && (index < ripv2_ROUTE_TABLE_SIZE))
  {
    removed_route = table->routes[index];
    table->routes[index] = NULL;
  }

  return removed_route;
}

/* ripv2_route_t * ripv2_route_table_get ( ripv2_route_table_t * table, int index ) */
ripv2_route_t* ripv2_route_table_get(ripv2_route_table_t* table, int index)
{
  if ((table != NULL) && (index >= 0) && (index < ripv2_ROUTE_TABLE_SIZE))
  {
    return table->routes[index];
  }
  return NULL;
}

int ripv2_route_table_size(ripv2_route_table_t* table)
{
  if (table == NULL)
  {
    return 0;
  }
  return ripv2_ROUTE_TABLE_SIZE;
}

/* ripv2_route_t * ripv2_route_table_lookup ( ... )
 * IMPORTANT: for RIP this function looks for an EXACT match of subnet and mask.
 * It is used to tell whether a received route already exists so it can be updated.
 */
ripv2_route_t* ripv2_route_table_lookup(ripv2_route_table_t* table,
                                        ipv4_addr_t subnet, ipv4_addr_t mask)
{
  if (table != NULL)
  {
    int i;
    for (i = 0; i < ripv2_ROUTE_TABLE_SIZE; i++)
    {
      ripv2_route_t* route = table->routes[i];
      if (route != NULL)
      {
        // Exact memory comparison of subnet and mask
        if (memcmp(route->subnet, subnet, IPv4_ADDR_SIZE) == 0 &&
          memcmp(route->mask, mask, IPv4_ADDR_SIZE) == 0)
        {
          return route; // Found
        }
      }
    }
  }
  return NULL; // Not found
}

/* void ripv2_route_table_print ( ripv2_route_table_t * table ) */
void ripv2_route_table_print(ripv2_route_table_t* table)
{
  if (table != NULL)
  {
    printf("\n--- RIPv2 Routing Table ---\n");
    int i;
    for (i = 0; i < ripv2_ROUTE_TABLE_SIZE; i++)
    {
      if (table->routes[i] != NULL)
      {
        ripv2_route_print(table->routes[i]);
      }
    }
    printf("---------------------------\n");
  }
}

/* Helpers for reading the file (simplified) */
int ripv2_route_table_read(char* filename, ripv2_route_table_t* table)
{
  FILE* file = fopen(filename, "r");
  if (file == NULL)
  {
    perror("Error opening RIPv2 routes file");
    return -1;
  }

  char line[256];
  int count = 0;

  while (fgets(line, sizeof(line), file))
  {
    // Ignore empty lines and comments (#)
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

    char subnet_str[32], mask_str[32], nh_str[32];
    int metric;

    // Parse the line
    if (sscanf(line, "%s %s %s %d", subnet_str, mask_str, nh_str, &metric) == 4)
    {
      ipv4_addr_t subnet, mask, nh;

      // Convert the strings to ipv4_addr_t
      if (ipv4_str_addr(subnet_str, subnet) == 0 &&
        ipv4_str_addr(mask_str, mask) == 0 &&
        ipv4_str_addr(nh_str, nh) == 0)
      {
        ripv2_route_t* new_route = ripv2_route_create(subnet, mask, nh, (uint32_t)metric);
        if (new_route)
        {
          /* No Response ever refreshes them, so without this flag the timers
             poison them after 180s and delete them after 300s. */
          new_route->is_static = 1;

          if (ripv2_route_table_add(table, new_route) != -1)
          {
            count++;
          }
          else
          {
            // If adding fails (e.g. table full), release the memory
            ripv2_route_free(new_route);
          }
        }
      }
    }
  }

  fclose(file);
  printf("[RIPv2] Loaded %d routes from %s\n", count, filename);
  return count;
}

