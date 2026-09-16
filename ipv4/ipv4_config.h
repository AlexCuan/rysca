#ifndef _IPv4_CONFIG_H
#define _IPv4_CONFIG_H

#include "ipv4.h"
#include <stdio.h>

/* int ipv4_config_read
 * ( char* filename, char ifname[], ipv4_addr_t addr, ipv4_addr_t netmask );
 *
 * DESCRIPTION:
 *   This function reads the specified IPv4 configuration file and returns the
 *   name of the interface, its IPv4 address and the subnet mask.
 *
 *   The memory for the interface name and the IPv4 addresses must have been
 *   reserved beforehand. At least 'IFACE_NAME_MAX_LENGTH' bytes must be
 *   reserved to store the interface name.
 *
 * PARAMETERS:
 *    'filename': Name of the configuration file to read.
 *      'ifname': Variable where the interface name read from the configuration
 *                file will be copied.
 *        'addr': Variable where the IPv4 address of the interface read from
 *                the configuration file will be copied.
 *     'netmask': Variable where the subnet mask read from the configuration
 *                file will be copied.
 *
 * RETURN VALUE:
 *   The function returns '0' if the configuration file was read correctly.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred while reading the
 *   configuration file.
 */
int ipv4_config_read
(char* filename, char ifname[], ipv4_addr_t addr, ipv4_addr_t netmask);


#endif /* _IPv4_CONFIG_H*/
