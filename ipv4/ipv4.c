
#include "ipv4.h"
#include "ipv4_route_table.h"
#include "ipv4_config.h"
#include "../eth/eth.h"
#include "../arp/arp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <timerms.h>


ipv4_addr_t IPv4_ZERO_ADDR = {0, 0, 0, 0};
ipv4_addr_t IPv4_BCAST_ADDR = {255, 255, 255, 255};

/* void ipv4_addr_str ( ipv4_addr_t addr, char* str );
 *
 * DESCRIPTION:
 *   This function generates a string representing the given IPv4 address.
 *
 * PARAMETERS:
 *   'addr': The IP address to represent as text.
 *    'str': Memory where the generated string is to be stored.
 *           At least 'IPv4_STR_MAX_LENGTH' bytes must be reserved.
 */
void ipv4_addr_str(ipv4_addr_t addr, char* str)
{
  if (str != NULL)
  {
    sprintf(str, "%d.%d.%d.%d",
            addr[0], addr[1], addr[2], addr[3]);
  }
}


/* int ipv4_str_addr ( char* str, ipv4_addr_t addr );
 *
 * DESCRIPTION:
 *   This function scans a string looking for an IPv4 address.
 *
 * PARAMETERS:
 *    'str': The string to process.
 *   'addr': Memory where the IPv4 address found is stored.
 *
 * RETURN VALUE:
 *   Returns 0 if the string represented an IPv4 address.
 *
 * ERRORS:
 *   The function returns -1 if the string did not represent an IPv4 address.
 */
int ipv4_str_addr(char* str, ipv4_addr_t addr)
{
  int err = -1;

  if (str != NULL)
  {
    unsigned int addr_int[IPv4_ADDR_SIZE];
    int len = sscanf(str, "%d.%d.%d.%d",
                     &addr_int[0], &addr_int[1],
                     &addr_int[2], &addr_int[3]);

    if (len == IPv4_ADDR_SIZE)
    {
      int i;
      for (i = 0; i < IPv4_ADDR_SIZE; i++)
      {
        addr[i] = (unsigned char)addr_int[i];
      }

      err = 0;
    }
  }

  return err;
}


/*
 * uint16_t ipv4_checksum ( unsigned char * data, int len )
 *
 * DESCRIPTION:
 *   This function computes the IP checksum of the given data.
 *
 * PARAMETERS:
 *   'data': Pointer to the data the checksum is computed over.
 *    'len': Length in bytes of the data.
 *
 * RETURN VALUE:
 *   The value of the computed checksum.
 */
uint16_t ipv4_checksum(unsigned char* data, int len)
{
  int i;
  uint16_t word16;
  unsigned int sum = 0;

  /* Make 16 bit words out of every two adjacent 8 bit words in the packet
   * and add them up */
  for (i = 0; i < len; i = i + 2)
  {
    word16 = ((data[i] << 8) & 0xFF00) + (data[i + 1] & 0x00FF);
    sum = sum + (unsigned int)word16;
  }

  /* Take only 16 bits out of the 32 bit sum and add up the carries */
  while (sum >> 16)
  {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  /* One's complement the result */
  sum = ~sum;

  return (uint16_t)sum;
}

/* Tells whether 'addr' is a broadcast address this layer must treat as such:
   the limited broadcast 255.255.255.255 or the broadcast directed at our own
   subnet (host part all ones). */
static int ipv4_is_broadcast(ipv4_layer_t* layer, ipv4_addr_t addr)
{
  if (memcmp(addr, IPv4_BCAST_ADDR, IPv4_ADDR_SIZE) == 0)
  {
    return 1;
  }

  int i;
  for (i = 0; i < IPv4_ADDR_SIZE; i++)
  {
    /* Same subnet as ourselves ... */
    if ((addr[i] & layer->netmask[i]) != (layer->addr[i] & layer->netmask[i]))
    {
      return 0;
    }
    /* ... and host part all ones. */
    if ((addr[i] | layer->netmask[i]) != 0xFF)
    {
      return 0;
    }
  }

  return 1;
}

/*
 * int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol, unsigned char * payload, int payload_len)
 *
 * DESCRIPTION:
 *   This function sends an IPv4 packet to the specified destination.
 *   It builds the IPv4 header, computes the checksum and sends the packet
 *   through the Ethernet layer.
 *   It looks up the appropriate route in the routing table to determine the
 *   next hop IP address. It then resolves the MAC address of that next hop
 *   using ARP before sending the packet.
 *
 * PARAMETERS:
 *   'layer': Pointer to the IPv4 layer structure used to send.
 *     'dst': Destination IPv4 address of the packet.
 * 'protocol': Upper layer protocol of the payload data (for example TCP, UDP).
 *  'payload': Pointer to the payload data to send.
 *'payload_len': Length in bytes of the payload data.
 *
 * RETURN VALUE:
 *   Returns the number of bytes sent if the send succeeded.
 *
 * ERRORS:
 *   Returns -1 if no route to the destination is found, if ARP resolution
 *   fails, or if an error occurs in the Ethernet layer.
 */
int ipv4_send(ipv4_layer_t* layer, ipv4_addr_t dst, uint8_t protocol, unsigned char* payload, int payload_len,
              int corrupt)
{
  mac_addr_t next_hop_mac;

  /* No fragmentation: the whole datagram must fit in a single frame. */
  if ((payload == NULL) || (payload_len < 0) ||
      (payload_len > (int)(ETH_MTU - sizeof(ipv4_header_t))))
  {
    fprintf(stderr, "ipv4_send(): longitud de payload invalida (%d)\n", payload_len);
    return -1;
  }

  int is_multicast = ((dst[0] & 0xF0) == 0xE0); // 224.0.0.0 a 239.255.255.255
  int is_broadcast = ipv4_is_broadcast(layer, dst);

  // Logic to determine the destination MAC
  if (is_broadcast)
  {
    // 1. IPv4 broadcast -> MAC broadcast mapping (FF:FF:FF:FF:FF:FF)
    memcpy(next_hop_mac, MAC_BCAST_ADDR, MAC_ADDR_SIZE);
  }
  else if (is_multicast)
  {
    // 2. IPv4 multicast -> MAC multicast mapping (01:00:5E:xx:xx:xx)
    // The low 23 bits of the IP are appended to the 01:00:5E prefix
    next_hop_mac[0] = 0x01;
    next_hop_mac[1] = 0x00;
    next_hop_mac[2] = 0x5E;
    next_hop_mac[3] = dst[1] & 0x7F; // Clears the top bit of the 2nd byte (bit 24 of the IP)
    next_hop_mac[4] = dst[2];
    next_hop_mac[5] = dst[3];
  }
  else
  {
    // 3. Unicast case: original behaviour (route + ARP)
    ipv4_route_t* route = ipv4_route_table_lookup(layer->routing_table, dst);
    if (!route)
    {
      return -1;
    }

    ipv4_addr_t next_hop_ip;
    if (memcmp(route->gateway_addr, IPv4_ZERO_ADDR, IPv4_ADDR_SIZE) == 0)
    {
      memcpy(next_hop_ip, dst, IPv4_ADDR_SIZE);
    }
    else
    {
      memcpy(next_hop_ip, route->gateway_addr, IPv4_ADDR_SIZE);
    }

    // Resolve the MAC using ARP
    if (arp_resolve(layer->iface, layer->addr, next_hop_ip, NULL, next_hop_mac) != 0)
    {
      return -1;
    }
  }

  const int header_len = sizeof(ipv4_header_t);
  const int total_len = header_len + payload_len;
  unsigned char* buffer = malloc(total_len);
  if (buffer == NULL) return -1;

  ipv4_header_t* ip_header = (ipv4_header_t*)buffer;
  ip_header->version_ihl = (4 << 4) | 5;
  ip_header->type_of_service = 0;
  ip_header->total_length = htons(total_len);
  ip_header->identification = 0;
  ip_header->flags_fragment_offset = 0;
  ip_header->time_to_live = 64; // Default TTL

  // For RIP multicast the TTL is normally 1
  if (is_multicast) ip_header->time_to_live = 1;

  ip_header->protocol = protocol;
  ip_header->header_checksum = 0;
  memcpy(ip_header->src_addr, layer->addr, IPv4_ADDR_SIZE);
  memcpy(ip_header->dest_addr, dst, IPv4_ADDR_SIZE);

  uint16_t checksum = ipv4_checksum((unsigned char*)ip_header, header_len);

  if (corrupt)
  {
    checksum ^= 0xFFFF;
    printf("DEBUG: Corrupting IPv4 Checksum\n");
  }

  ip_header->header_checksum = htons(checksum);

  memcpy(buffer + header_len, payload, payload_len);

  int bytes_sent = eth_send(layer->iface, next_hop_mac, ETH_TYPE_IPV4, buffer, total_len);

  free(buffer);

  if (bytes_sent < 0)
  {
    return -1;
  }

  /* Return the useful bytes handed over by the upper layer rather than the
     datagram size, so each layer reports its own payload. */
  return payload_len;
}

/*
 * ipv4_layer_t * ipv4_open(char * file_conf, char * file_conf_route)
 *
 * DESCRIPTION:
 *   This function initialises the IPv4 layer.
 *   It creates and configures an ipv4_layer_t structure, which includes
 *   creating a routing table, reading the network configuration (IP address,
 *   subnet mask) from a file and loading the routing table from another file.
 *   Finally it initialises the underlying Ethernet layer.
 *
 * PARAMETERS:
 *   'file_conf': Path to the configuration file containing the interface,
 *                the IPv4 address and the subnet mask.
 *   'file_conf_route': Path to the file containing the routing table.
 *
 * RETURN VALUE:
 *   Returns a pointer to the initialised ipv4_layer_t structure if the
 *   operation succeeded.
 *
 * ERRORS:
 *   Returns NULL if an error occurs during memory allocation, while reading
 *   the configuration files or while initialising the Ethernet layer.
 */
ipv4_layer_t* ipv4_open(char* file_conf, char* file_conf_route)
{
  ipv4_layer_t* layer = malloc(sizeof(ipv4_layer_t));
  if (!layer)
  {
    perror("malloc ipv4_layer_t");
    return NULL;
  }

  // Create the routing table
  layer->routing_table = ipv4_route_table_create();
  if (!layer->routing_table)
  {
    free(layer);
    return NULL;
  }

  // Read the addresses and subnet from file_conf
  char ifname[IFACE_NAME_MAX_LENGTH];
  if (ipv4_config_read(file_conf, ifname, layer->addr, layer->netmask) != 0)
  {
    fprintf(stderr, "Error reading IPv4 config file %s\n", file_conf);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  // Read the IP forwarding table from file_conf_route
  if (ipv4_route_table_read(file_conf_route, layer->routing_table) < 0)
  {
    fprintf(stderr, "Error reading IPv4 route table file %s\n", file_conf_route);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  // Initialise the Ethernet layer with eth_open()
  layer->iface = eth_open(ifname);
  if (!layer->iface)
  {
    fprintf(stderr, "Error opening Ethernet interface %s\n", ifname);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  return layer;
}

/*
 * int ipv4_close(ipv4_layer_t * layer)
 *
 * DESCRIPTION:
 *   This function releases the resources associated with an IPv4 layer.
 *   It closes the Ethernet interface, frees the routing table and frees the
 *   memory of the ipv4_layer_t structure.
 *
 * PARAMETERS:
 *   'layer': Pointer to the IPv4 layer structure to close.
 *
 * RETURN VALUE:
 *   Returns 0 if the operation succeeded.
 *
 * ERRORS:
 *   Returns -1 if the 'layer' pointer is NULL.
 */
int ipv4_close(ipv4_layer_t* layer)
{
  if (!layer)
  {
    fprintf(stderr, "ipv4_close: 'layer' cannot be NULL\n");
    return -1;
  }

  if (layer->iface)
  {
    eth_close(layer->iface);
  }
  if (layer->routing_table)
  {
    ipv4_route_table_free(layer->routing_table);
  }
  free(layer);

  return 0;
}

// Helper to print hex data
void print_hex(unsigned char* data, int len)
{
  for (int i = 0; i < len; i++)
  {
    printf("%02x ", data[i]);
    if ((i + 1) % 16 == 0)
    {
      printf("\n");
    }
  }
  printf("\n");
}

/*
 * int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol, unsigned char buffer[], ipv4_addr_t sender, int buf_len, long int timeout)
 *
 * DESCRIPTION:
 *   This function receives IPv4 packets. It waits for an IPv4 packet to arrive
 *   on the network interface associated with the IPv4 layer.
 *   It performs several validations on the received packet, including the IP
 *   version, the header length, the checksum and the destination address. If
 *   the packet is valid and addressed to this interface and protocol, it
 *   copies the payload into a buffer supplied by the caller.
 *
 * PARAMETERS:
 *   'layer': Pointer to the IPv4 layer structure the packet is received on.
 * 'protocol': Expected upper layer protocol (for example TCP, UDP).
 *  'buffer': Buffer where the payload of the received packet is copied.
 *  'sender': Array where the IPv4 address of the sender is stored.
 * 'buf_len': Maximum length of the buffer supplied for the payload.
 * 'timeout': Maximum time in milliseconds the function waits for a packet.
 *
 * RETURN VALUE:
 *   Returns the length of the received payload if the packet was processed
 *   successfully.
 *
 * ERRORS:
 *   Returns -1 if an error occurs in the Ethernet layer.
 */
int ipv4_recv(ipv4_layer_t* layer, uint8_t protocol,
              unsigned char buffer[], ipv4_addr_t sender, ipv4_addr_t dest,
              int buf_len, long int timeout)
{
  mac_addr_t src_mac;
  unsigned char eth_buffer[ETH_MTU];
  int payload_len;

  /* The timer spans the whole loop: discarding a frame must not grant a fresh
     full timeout, or the wait stretches on indefinitely as long as packets we
     cannot use keep arriving. */
  timerms_t timer;
  timerms_reset(&timer, timeout);

  while (1)
  {
    long int time_left = timerms_left(&timer);

    payload_len = eth_recv(layer->iface, src_mac, ETH_TYPE_IPV4, eth_buffer, sizeof(eth_buffer), time_left);
    if (payload_len < 0)
    {
      return -1; // Error
    }
    if (payload_len == 0)
    {
      return 0; // Timeout
    }


    if (payload_len < (int)sizeof(ipv4_header_t))
    {
      // Packet too small to be a valid IPv4 packet
      continue;
    }

    ipv4_header_t* ip_header = (ipv4_header_t*)eth_buffer;


    memcpy(sender, ip_header->src_addr, IPv4_ADDR_SIZE);

    if (dest != NULL)
    {
      memcpy(dest, ip_header->dest_addr, IPv4_ADDR_SIZE);
    }

    // Validate Version and Header Length
    if ((ip_header->version_ihl >> 4) != 4)
    {
      fprintf(stderr, "IPv4 Recv: Incorrect IP version\n");
      continue;
    }
    unsigned int header_len = (ip_header->version_ihl & 0x0F) * 4;
    if ((header_len < sizeof(ipv4_header_t)) || (header_len > (unsigned int)payload_len))
    {
      fprintf(stderr, "IPv4 Recv: Invalid header length\n");
      continue;
    }


    // 2. Validate Checksum
    uint16_t received_checksum = ip_header->header_checksum;
    ip_header->header_checksum = 0;
    uint16_t calculated_checksum = ipv4_checksum((unsigned char*)ip_header, header_len);
    ip_header->header_checksum = received_checksum;

    if (received_checksum != htons(calculated_checksum))
    {
      fprintf(stderr, "IPv4 Recv: Invalid checksum\n");
      continue;
    }

    if (ip_header->protocol != protocol)
    {
      // printf("Ignored packet with protocol %d (Expected %d)\n", ip_header->protocol, protocol);
      continue;
    }

    // 3. Check destination address
    int is_for_me = (memcmp(ip_header->dest_addr, layer->addr, IPv4_ADDR_SIZE) == 0);
    int is_multicast = ((ip_header->dest_addr[0] & 0xF0) == 0xE0);
    int is_broadcast = ipv4_is_broadcast(layer, ip_header->dest_addr);


#ifdef NET_DEBUG
    printf("[IPv4 DEBUG] Paquete recibido para %d.%d.%d.%d (Mio:%d, Multi:%d)\n",
           ip_header->dest_addr[0], ip_header->dest_addr[1],
           ip_header->dest_addr[2], ip_header->dest_addr[3],
           is_for_me, is_multicast);
#endif

    if (!is_for_me && !is_multicast && !is_broadcast)
    {
      continue;
    }

    /* 'total_length' comes from the network: without bounding it against the
       bytes Ethernet delivered, data that was never received gets copied,
       reading past 'eth_buffer' when the header carries options. Ethernet
       padding can make payload_len larger, but never smaller. */
    const int ip_total_len = ntohs(ip_header->total_length);
    if ((ip_total_len < (int)header_len) || (ip_total_len > payload_len))
    {
      fprintf(stderr, "IPv4 Recv: Invalid total length\n");
      continue;
    }

    const int ip_payload_len = ip_total_len - header_len;

    if (ip_payload_len <= 0)
    {
      continue;
    }

    //  Copy sender IP
    memcpy(sender, ip_header->src_addr, IPv4_ADDR_SIZE);

    // Copy payload to user buffer
    // Ternary: (condition) ? (value_if_true) : (value_if_false);
    // Prevent buffer overflow
    const int len_to_copy = (ip_payload_len > buf_len) ? buf_len : ip_payload_len;
    unsigned char* payload = eth_buffer + header_len;
    memcpy(buffer, payload, len_to_copy);


    return len_to_copy;
  }
}
