#include "ipv4_route_table.h"
#include "ipv4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


/* ipv4_route_t * ipv4_route_create
 * ( ipv4_addr_t subnet, ipv4_addr_t mask, char* iface, ipv4_addr_t gw );
 *
 * DESCRIPTION:
 *   This function creates an IPv4 route with the given parameters: subnet
 *   address, mask, interface name and next hop address.
 *
 *   This function reserves memory for the created structure. The
 *   'ipv4_route_free()' function must be used to release that memory.
 *
 * PARAMETERS:
 *   'subnet': IPv4 address of the destination subnet of the new route.
 *     'mask': Mask of the destination subnet of the new route.
 *    'iface': Name of the interface used to reach the destination subnet of
 *             the new route.
 *             It must be at most 'IFACE_NAME_LENGTH' characters long.
 *       'gw': IPv4 address of the router used to reach the destination subnet
 *             of the new route.
 *
 * RETURN VALUE:
 *   The function returns a pointer to the created route.
 *
 * ERRORS:
 *   The function returns 'NULL' if it was not possible to reserve memory to
 *   create the route.
 */
ipv4_route_t* ipv4_route_create
(ipv4_addr_t subnet, ipv4_addr_t mask, char* iface, ipv4_addr_t gw)
{
  if ((subnet == NULL) || (mask == NULL) || (iface == NULL) || (gw == NULL))
  {
    return NULL;
  }

  ipv4_route_t* route = (ipv4_route_t*)malloc(sizeof(struct ipv4_route));

  if (route != NULL)
  {
    memcpy(route->subnet_addr, subnet, IPv4_ADDR_SIZE);
    memcpy(route->subnet_mask, mask, IPv4_ADDR_SIZE);
    /* strncpy does not terminate the string if 'iface' fills the buffer */
    strncpy(route->iface, iface, IFACE_NAME_MAX_LENGTH - 1);
    route->iface[IFACE_NAME_MAX_LENGTH - 1] = '\0';
    memcpy(route->gateway_addr, gw, IPv4_ADDR_SIZE);
  }

  return route;
}


/* int ipv4_route_lookup ( ipv4_route_t * route, ipv4_addr_t addr );
 *
 * DESCRIPTION:
 *   This function tells whether the given IPv4 address belongs to the given
 *   subnet. In that case it returns the length of the subnet mask.
 *
 * PARAMETERS:
 *   'route': Route to the subnet to check.
 *    'addr': Destination IPv4 address.
 *
 * RETURN VALUE:
 *   If the IPv4 address belongs to the subnet of the given route, it must
 *   return a positive number indicating the length of the subnet prefix, that
 *   is, the number of bits set to one in the subnet mask.
 *   The function returns '-1' if the IPv4 address does not belong to the
 *   subnet pointed to by the given route.
 */
int ipv4_route_lookup(ipv4_route_t* route, ipv4_addr_t addr)
{
  // Make sure the pointers are not null
  if (route == NULL || addr == NULL)
  {
    return -1;
  }

  //    Check whether the address belongs to the subnet of the route.
  //    The condition is: (addr & subnet_mask) == subnet_addr
  for (int i = 0; i < IPv4_ADDR_SIZE; i++)
  {
    if ((addr[i] & route->subnet_mask[i]) != route->subnet_addr[i])
    {
      // If the condition fails for any byte, the address is not in the subnet.
      return -1;
    }
  }

  // 2. If it belongs, compute the prefix length (count the '1' bits in the mask).
  int prefix_length = 0;
  for (int i = 0; i < IPv4_ADDR_SIZE; i++)
  {
    unsigned char mask_byte = route->subnet_mask[i];
    // Count the '1' bits in each byte of the mask
    while (mask_byte > 0)
    {
      // The 'n & (n-1)' operation clears the least significant '1' bit.
      mask_byte &= (mask_byte - 1);
      prefix_length++;
    }
  }

  return prefix_length;
}

/* void ipv4_route_print ( ipv4_route_t * route );
 *
 * DESCRIPTION:
 *   This function prints the given route to standard output.
 *
 * PARAMETERS:
 *   'route': Route to print.
 */
void ipv4_route_print(ipv4_route_t* route)
{
  if (route != NULL)
  {
    char subnet_str[IPv4_STR_MAX_LENGTH];
    ipv4_addr_str(route->subnet_addr, subnet_str);
    char mask_str[IPv4_STR_MAX_LENGTH];
    ipv4_addr_str(route->subnet_mask, mask_str);
    char* iface_str = route->iface;
    char gw_str[IPv4_STR_MAX_LENGTH];
    ipv4_addr_str(route->gateway_addr, gw_str);

    printf("%s/%s via %s dev %s", subnet_str, mask_str, gw_str, iface_str);
  }
}


/* void ipv4_route_free ( ipv4_route_t * route );
 *
 * DESCRIPTION:
 *   This function releases the memory reserved for the given route, which was
 *   created with 'ipv4_route_create()'.
 *
 * PARAMETERS:
 *   'route': Route to release.
 */
void ipv4_route_free(ipv4_route_t* route)
{
  if (route != NULL)
  {
    free(route);
  }
}

/* ipv4_route_t* ipv4_route_read ( char* filename, int linenum, char * line )
 *
 * DESCRIPTION:
 *   This function creates an IPv4 route from the given line of the route
 *   table file.
 *
 * PARAMETERS:
 *   'filename': Name of the route table file.
 *    'linenum': Line number within the route table file.
 *       'line': Line of the route table file to process.
 *
 * RETURN VALUE:
 *   The route read, or NULL if no route was read.
 *
 * ERRORS:
 *   The function prints an error message and returns NULL if an error
 *   occurred while reading the route.
 */
ipv4_route_t* ipv4_route_read(char* filename, int linenum, char* line)
{
  ipv4_route_t* route = NULL;

  char subnet_str[256];
  char mask_str[256];
  char iface_name[256];
  char gw_str[256];

  /* Parse line: Format "<subnet> <mask> <iface> <gw>\n" */
  int params = sscanf(line, "%s %s %s %s\n",
                      subnet_str, mask_str, iface_name, gw_str);
  if (params != 4)
  {
    fprintf(stderr, "%s:%d: Invalid IPv4 Route format: '%s' (%d params)\n",
            filename, linenum, line, params);
    fprintf(stderr,
            "%s:%d: Format must be: <subnet> <mask> <iface> <gw>\n",
            filename, linenum);
    return NULL;
  }

  /* Parse IPv4 route subnet address */
  ipv4_addr_t subnet;
  int err = ipv4_str_addr(subnet_str, subnet);
  if (err == -1)
  {
    fprintf(stderr, "%s:%d: Invalid <subnet> value: '%s'\n",
            filename, linenum, subnet_str);
    return NULL;
  }

  /* Parse IPv4 route subnet mask */
  ipv4_addr_t mask;
  err = ipv4_str_addr(mask_str, mask);
  if (err == -1)
  {
    fprintf(stderr, "%s:%d: Invalid <mask> value: '%s'\n",
            filename, linenum, mask_str);
    return NULL;
  }

  /* Parse IPv4 route gateway */
  ipv4_addr_t gateway;
  err = ipv4_str_addr(gw_str, gateway);
  if (err == -1)
  {
    fprintf(stderr, "%s:%d: Invalid <gw> value: '%s'\n",
            filename, linenum, gw_str);
    return NULL;
  }

  /* Create new route with parsed parameters */
  route = ipv4_route_create(subnet, mask, iface_name, gateway);
  if (route == NULL)
  {
    fprintf(stderr, "%s:%d: Error creating the new route\n",
            filename, linenum);
  }

  return route;
}


/* void ipv4_route_output ( ipv4_route_t * route, FILE * out );
 *
 * DESCRIPTION:
 *   This function prints the given IPv4 route to the given output.
 *
 * PARAMETERS:
 *      'route': Route to print.
 *     'header': '0' to print a line with the route header.
 *        'out': Output to print the route to.
 *
 * RETURN VALUE:
 *   The function returns '0' if the route was printed correctly.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred while writing to the given
 *   output.
 */
int ipv4_route_output(ipv4_route_t* route, int header, FILE* out)
{
  int err;

  if (header == 0)
  {
    err = fprintf(out, "# SubnetAddr  \tSubnetMask    \tIface  \tGateway\n");
    if (err < 0)
    {
      return -1;
    }
  }

  char subnet_str[IPv4_STR_MAX_LENGTH];
  char mask_str[IPv4_STR_MAX_LENGTH];
  char* ifname = NULL;
  char gw_str[IPv4_STR_MAX_LENGTH];

  if (route != NULL)
  {
    ipv4_addr_str(route->subnet_addr, subnet_str);
    ipv4_addr_str(route->subnet_mask, mask_str);
    ifname = route->iface;
    ipv4_addr_str(route->gateway_addr, gw_str);

    err = fprintf(out, "%-15s\t%-15s\t%s\t%-15s\n",
                  subnet_str, mask_str, ifname, gw_str);
    if (err < 0)
    {
      return -1;
    }
  }

  return 0;
}


struct ipv4_route_table
{
  ipv4_route_t* routes[IPv4_ROUTE_TABLE_SIZE];
};

/* ipv4_route_table_t * ipv4_route_table_create();
 *
 * DESCRIPTION:
 *   This function creates an empty IPv4 route table.
 *
 *   This function reserves memory for the created route table; to release it
 *   the 'ipv4_route_table_free()' function must be called.
 *
 * RETURN VALUE:
 *   The function returns a pointer to the created route table.
 *
 * ERRORS:
 *   The function returns 'NULL' if it was not possible to reserve memory to
 *   create the route table.
 */
ipv4_route_table_t* ipv4_route_table_create()
{
  ipv4_route_table_t* table;

  table = (ipv4_route_table_t*)malloc(sizeof(struct ipv4_route_table));
  if (table != NULL)
  {
    int i;
    for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
    {
      table->routes[i] = NULL;
    }
  }

  return table;
}


/* int ipv4_route_table_add ( ipv4_route_table_t * table,
 *                            ipv4_route_t * route );
 * DESCRIPTION:
 *   This function adds the given route in the first free position of the
 *   route table.
 *
 * PARAMETERS:
 *   'table': Table to add the given route to.
 *   'route': Route to add to the route table.
 *
 * RETURN VALUE:
 *   The function returns the index of the position
 *   [0,IPv4_ROUTE_TABLE_SIZE-1] where the given route was added.
 *
 * ERRORS:
 *   The function returns '-1' if it was not possible to add the given route.
 */
int ipv4_route_table_add(ipv4_route_table_t* table, ipv4_route_t* route)
{
  int route_index = -1;

  if (table != NULL)
  {
    /* Find an empty place in the route table */
    int i;
    for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
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


/* ipv4_route_t * ipv4_route_table_remove ( ipv4_route_table_t * table,
 *                                          int index );
 *
 * DESCRIPTION:
 *   This function removes the route stored at the given position of the route
 *   table.
 *
 *   This function does NOT release the memory reserved for the removed route.
 *   To do so, the 'ipv4_route_free()' function must be used on the returned
 *   route.
 *
 * PARAMETERS:
 *   'table': Route table to remove a route from.
 *   'index': Index of the route to remove. It must take a value between
 *            [0, IPv4_ROUTE_TABLE_SIZE-1].
 *
 * RETURN VALUE:
 *   This function returns the route that was stored at the given position.
 *
 * ERRORS:
 *   This function returns 'NULL' if the route could not be removed, or if no
 *   route existed at that position.
 */
ipv4_route_t* ipv4_route_table_remove(ipv4_route_table_t* table, int index)
{
  ipv4_route_t* removed_route = NULL;

  if ((table != NULL) && (index >= 0) && (index < IPv4_ROUTE_TABLE_SIZE))
  {
    removed_route = table->routes[index];
    table->routes[index] = NULL;
  }

  return removed_route;
}


/* ipv4_route_t * ipv4_route_table_lookup ( ipv4_route_table_t * table,
 *                                          ipv4_addr_t addr );
 *
 * DESCRIPTION:
 *   This function returns the best route stored in the route table to reach
 *   the given destination IPv4 address.
 *
 *   This function walks the whole route table looking for routes that contain
 *   the given IPv4 address, using the 'ipv4_route_lookup()' function. Of all
 *   the possible routes, the one with the most specific prefix is returned,
 *   that is, the one with the longest subnet mask.
 *
 * PARAMETERS:
 *   'table': Route table to look the destination IPv4 address up in.
 *    'addr': Destination IPv4 address to look up.
 *
 * RETURN VALUE:
 *   This function returns the most specific route to reach the given IPv4
 *   address.
 *
 * ERRORS:
 *   This function returns 'NULL' if no route exists to reach the given
 *   address, or if the lookup could not be performed.
 */
ipv4_route_t* ipv4_route_table_lookup(ipv4_route_table_t* table,
                                      ipv4_addr_t addr)
{
  ipv4_route_t* best_route = NULL;
  int best_route_prefix = -1;

  if (table != NULL)
  {
    int i;
    for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
    {
      ipv4_route_t* route_i = table->routes[i];
      if (route_i != NULL)
      {
        int route_i_lookup = ipv4_route_lookup(route_i, addr);
        if (route_i_lookup > best_route_prefix)
        {
          best_route = route_i;
          best_route_prefix = route_i_lookup;
        }
      }
    }
  }

  return best_route;
}


/* ipv4_route_t * ipv4_route_table_get ( ipv4_route_table_t * table, int index );
 *
 * DESCRIPTION:
 *   This function returns the route stored at the given position of the route
 *   table.
 *
 * PARAMETERS:
 *   'table': Route table to obtain a route from.
 *   'index': Index of the route queried. It must take a value between
 *            [0, IPv4_ROUTE_TABLE_SIZE-1].
 *
 * RETURN VALUE:
 *   This function returns the route stored at the given position of the route
 *   table.
 *
 * ERRORS:
 *   This function returns 'NULL' if the route table could not be queried, or
 *   if no route exists at that position.
 */
ipv4_route_t* ipv4_route_table_get(ipv4_route_table_t* table, int index)
{
  ipv4_route_t* route = NULL;

  if ((table != NULL) && (index >= 0) && (index < IPv4_ROUTE_TABLE_SIZE))
  {
    route = table->routes[index];
  }

  return route;
}


/* int ipv4_route_table_find ( ipv4_route_table_t * table, ipv4_addr_t subnet,
 *                                                         ipv4_addr_t mask );
 *
 * DESCRIPTION:
 *   This function returns the index of the route to reach the given subnet.
 *
 * PARAMETERS:
 *    'table': Route table to look the subnet up in.
 *   'subnet': Address of the subnet to look up.
 *     'mask': Mask of the subnet to look up.
 *
 * RETURN VALUE:
 *   This function returns the position of the route table holding the route
 *   that points to the given subnet.
 *
 * ERRORS:
 *   The function returns '-1' if the given route was not found, or '-2' if the
 *   lookup could not be performed.
 */
int ipv4_route_table_find
(ipv4_route_table_t* table, ipv4_addr_t subnet, ipv4_addr_t mask)
{
  int route_index = -2;

  if (table != NULL)
  {
    route_index = -1;
    int i;
    for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
    {
      ipv4_route_t* route_i = table->routes[i];
      if (route_i != NULL)
      {
        int same_subnet =
          (memcmp(route_i->subnet_addr, subnet, IPv4_ADDR_SIZE) == 0);
        int same_mask =
          (memcmp(route_i->subnet_mask, mask, IPv4_ADDR_SIZE) == 0);

        if (same_subnet && same_mask)
        {
          route_index = i;
          break;
        }
      }
    }
  }

  return route_index;
}


/* void ipv4_route_table_free ( ipv4_route_table_t * table );
 *
 * DESCRIPTION:
 *   This function releases the memory reserved for the given route table,
 *   including every route stored in it, by means of the 'ipv4_route_free()'
 *   function.
 *
 * PARAMETERS:
 *   'table': Route table to delete.
 */
void ipv4_route_table_free(ipv4_route_table_t* table)
{
  if (table != NULL)
  {
    int i;
    for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
    {
      ipv4_route_t* route_i = table->routes[i];
      if (route_i != NULL)
      {
        table->routes[i] = NULL;
        ipv4_route_free(route_i);
      }
    }
    free(table);
  }
}


/* int ipv4_route_table_read ( char * filename, ipv4_route_table_t * table );
 *
 * DESCRIPTION:
 *   This function reads the given file and adds the static IPv4 routes it
 *   contains to the given route table.
 *
 * PARAMETERS:
 *   'filename': Name of the file with IPv4 routes to read.
 *      'table': Route table to add the routes read to.
 *
 * RETURN VALUE:
 *   The function returns the number of routes read and added to the table, or
 *   '0' if no route was read.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred while reading the routes
 *   file.
 */
int ipv4_route_table_read(char* filename, ipv4_route_table_t* table)
{
  int read_routes = 0;

  FILE* routes_file = fopen(filename, "r");
  if (routes_file == NULL)
  {
    fprintf(stderr, "Error opening input IPv4 Routes file \"%s\": %s.\n",
            filename, strerror(errno));
    return -1;
  }

  int linenum = 0;
  char line_buf[1024];
  int err = 0;

  while ((!feof(routes_file)) && (err == 0))
  {
    linenum++;

    /* Read next line of file */
    char* line = fgets(line_buf, 1024, routes_file);
    if (line == NULL)
    {
      break;
    }

    /* If this line is empty or a comment, just ignore it */
    if ((line_buf[0] == '\n') || (line_buf[0] == '#'))
    {
      err = 0;
      continue;
    }

    /* Parse route from line */
    ipv4_route_t* new_route = ipv4_route_read(filename, linenum, line);
    if (new_route == NULL)
    {
      err = -1;
      break;
    }

    /* Add new route to Route Table */
    if (table != NULL)
    {
      err = ipv4_route_table_add(table, new_route);
      if (err >= 0)
      {
        err = 0;
        read_routes++;
      }
      else
      {
        fprintf(stderr, "%s:%d: Route table full, route discarded\n",
                filename, linenum);
        ipv4_route_free(new_route);
      }
    }
    else
    {
      ipv4_route_free(new_route);
    }
  } /* while() */

  if (err == -1)
  {
    read_routes = -1;
  }

  /* Close IP Route Table file */
  fclose(routes_file);

  return read_routes;
}


/* void ipv4_route_table_output ( ipv4_route_table_t * table, FILE * out );
 *
 * DESCRIPTION:
 *   This function prints the given IPv4 route table to the given output.
 *
 * PARAMETERS:
 *      'table': Route table to print.
 *        'out': Output to print the route table to.
 *
 * RETURN VALUE:
 *   The function returns the number of routes printed to the given output, or
 *   '0' if the route table was empty.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred while writing to the given
 *   output.
 */
int ipv4_route_table_output(ipv4_route_table_t* table, FILE* out)
{
  int err;
  int num_routes = 0;

  int i;
  for (i = 0; i < IPv4_ROUTE_TABLE_SIZE; i++)
  {
    ipv4_route_t* route_i = ipv4_route_table_get(table, i);
    if (route_i != NULL)
    {
      /* The header depends on whether a route has been written yet, not on the
         index: if position 0 is empty it must not be lost. */
      err = ipv4_route_output(route_i, num_routes, out);
      if (err == -1)
      {
        return -1;
      }
      num_routes++;
    }
  }

  return num_routes;
}


/* int ipv4_route_table_write ( ipv4_route_table_t * table, char * filename );
 *
 * DESCRIPTION:
 *   This function stores the given IPv4 route table in the given file.
 *
 * PARAMETERS:
 *      'table': Route table to store.
 *   'filename': Name of the file to store the route table in.
 *
 * RETURN VALUE:
 *   The function returns the number of routes stored in the routes file, or
 *   '0' if the route table was empty.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred while writing the routes
 *   file.
 */
int ipv4_route_table_write(ipv4_route_table_t* table, char* filename)
{
  int num_routes = 0;

  FILE* routes_file = fopen(filename, "w");
  if (routes_file == NULL)
  {
    fprintf(stderr, "Error opening output IPv4 Routes file \"%s\": %s.\n",
            filename, strerror(errno));
    return -1;
  }

  fprintf(routes_file, "# %s\n", filename);
  fprintf(routes_file, "#\n");

  if (table != NULL)
  {
    num_routes = ipv4_route_table_output(table, routes_file);
    if (num_routes == -1)
    {
      fprintf(stderr, "Error writing IPv4 Routes file \"%s\": %s.\n",
              filename, strerror(errno));
      return -1;
    }
  }

  fclose(routes_file);

  return num_routes;
}


/* void ipv4_route_table_print ( ipv4_route_table_t * table );
 *
 * DESCRIPTION:
 *   This function prints the given IPv4 route table to standard output.
 *
 * PARAMETERS:
 *      'table': Route table to print.
 */
void ipv4_route_table_print(ipv4_route_table_t* table)
{
  if (table != NULL)
  {
    ipv4_route_table_output(table, stdout);
  }
}
