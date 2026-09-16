#ifndef _IPv4_ROUTE_TABLE_H
#define _IPv4_ROUTE_TABLE_H


/* This structure stores the basic information about the route to a subnet.
 * It includes the address and mask of the destination subnet, the name of the
 * outgoing interface, and the IP address of the next hop.
 *
 * Use the 'ipv4_route_create()' and 'ipv4_route_free()' methods to create and
 * release this structure. In addition, the implementation of the
 * 'ipv4_route_lookup()' method must be completed.
 *
 * Building the route table of a routing protocol will probably require adding
 * more fields to this structure, as well as modifying the associated
 * functions.
 */

/* Forward declarations */
typedef unsigned char ipv4_addr_t[4];

/* Maximum length of a network interface name */
#define IFACE_NAME_MAX_LENGTH 32

typedef struct ipv4_route
{
    ipv4_addr_t subnet_addr;
    ipv4_addr_t subnet_mask;
    char iface[IFACE_NAME_MAX_LENGTH];
    ipv4_addr_t gateway_addr;
} ipv4_route_t;


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
 *             the new route. It must be at most 'IFACE_NAME_MAX_LENGTH'
 *             characters long.
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
(ipv4_addr_t subnet, ipv4_addr_t mask, char* iface, ipv4_addr_t gw);


/* int ipv4_route_lookup ( ipv4_route_t * route, ipv4_addr_t addr );
 *
 * DESCRIPTION:
 *   This function tells whether the given IPv4 address belongs to the given
 *   subnet. In that case it returns the length of the subnet mask.
 *
 * *********************************************************************
 * * This function is NOT implemented; it must be implemented for the  *
 * * 'ipv4_route_table_lookup()' function to work correctly.           *
 * *********************************************************************
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
int ipv4_route_lookup(ipv4_route_t* route, ipv4_addr_t addr);


/* void ipv4_route_print ( ipv4_route_t * route );
 *
 * DESCRIPTION:
 *   This function prints the given route to standard output.
 *
 * PARAMETERS:
 *   'route': Route to print.
 */
void ipv4_route_print(ipv4_route_t* route);


/* void ipv4_route_free ( ipv4_route_t * route );
 *
 * DESCRIPTION:
 *   This function releases the memory reserved for the given route, which was
 *   created with 'ipv4_route_create()'.
 *
 * PARAMETERS:
 *   'route': Route whose memory is to be released.
 */
void ipv4_route_free(ipv4_route_t* route);


/* Maximum number of entries in the IPv4 route table */
#define IPv4_ROUTE_TABLE_SIZE 256


/* Definition of the opaque structure modelling an IPv4 route table.
 * The entries of the route table are indexed, and that index may take a value
 * between 0 and 'IPv4_ROUTE_TABLE_SIZE - 1'. This implementation does not
 * allow duplicate routes (e.g. the same route with different administrative
 * distances), so before adding a new route it must be checked that it does
 * not already exist.
 *
 * This structure must never be created directly. Instead, the
 * 'ipv4_route_table_create()' and 'ipv4_route_table_free()' functions must be
 * used to create and release it, respectively.
 *
 * Once the route table has been created, use 'ipv4_route_table_get()' to
 * access the route at a given position. Routes can also be added
 * ['ipv4_route_table_add()'] and removed ['ipv4_route_table_remove()'], and a
 * particular subnet can be looked up ['ipv4_route_table_find()'].
 * 'ipv4_route_table_lookup()' is the most important function of the route
 * table, as it returns the route to reach the given destination IPv4 address.
 *
 * In addition, the 'ipv4_route_table_read()', 'ipv4_route_table_write()' and
 * 'ipv4_route_table_print()' functions respectively read/write the route table
 * from/to a file and print it to standard output.
 */
typedef struct ipv4_route_table ipv4_route_table_t;


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
ipv4_route_table_t* ipv4_route_table_create();


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
 *   [0, IPv4_ROUTE_TABLE_SIZE-1] where the given route was added.
 *
 * ERRORS:
 *   The function returns '-1' if it was not possible to add the given route.
 */
int ipv4_route_table_add(ipv4_route_table_t* table, ipv4_route_t* route);


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
 *   This function returns the route that was stored at the given position
 *   before being removed.
 *
 * ERRORS:
 *   This function returns 'NULL' if the route could not be removed, or if no
 *   route existed at that position.
 */
ipv4_route_t* ipv4_route_table_remove(ipv4_route_table_t* table, int index);


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
                                      ipv4_addr_t addr);


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
 *   This function returns 'NULL' if no route exists at that position, or if
 *   the route table could not be queried.
 */
ipv4_route_t* ipv4_route_table_get(ipv4_route_table_t* table, int index);


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
(ipv4_route_table_t* table, ipv4_addr_t subnet, ipv4_addr_t mask);


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
void ipv4_route_table_free(ipv4_route_table_t* table);


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
int ipv4_route_table_read(char* filename, ipv4_route_table_t* table);


/* void ipv4_route_table_print ( ipv4_route_table_t * table );
 *
 * DESCRIPTION:
 *   This function prints the given IPv4 route table to standard output.
 *
 * PARAMETERS:
 *      'table': Route table to print.
 */
void ipv4_route_table_print(ipv4_route_table_t* table);


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
int ipv4_route_table_write(ipv4_route_table_t* table, char* filename);


#endif /* _IPv4_ROUTE_TABLE_H */
