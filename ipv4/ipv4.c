#include "ipv4.h"
#include "ipv4_route_table.h"
#include "ipv4_config.h"
#include "../eth/eth.h"
#include "../arp/arp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>


/* Dirección IPv4 a cero: "0.0.0.0" */
ipv4_addr_t IPv4_ZERO_ADDR = { 0, 0, 0, 0 };


/* void ipv4_addr_str ( ipv4_addr_t addr, char* str );
 *
 * DESCRIPCIÓN:
 *   Esta función genera una cadena de texto que representa la dirección IPv4
 *   indicada.
 *
 * PARÁMETROS:
 *   'addr': La dirección IP que se quiere representar textualente.
 *    'str': Memoria donde se desea almacenar la cadena de texto generada.
 *           Deben reservarse al menos 'IPv4_STR_MAX_LENGTH' bytes.
 */
void ipv4_addr_str ( ipv4_addr_t addr, char* str )
{
  if (str != NULL) {
    sprintf(str, "%d.%d.%d.%d",
            addr[0], addr[1], addr[2], addr[3]);
  }
}


/* int ipv4_str_addr ( char* str, ipv4_addr_t addr );
 *
 * DESCRIPCIÓN:
 *   Esta función analiza una cadena de texto en busca de una dirección IPv4.
 *
 * PARÁMETROS:
 *    'str': La cadena de texto que se desea procesar.
 *   'addr': Memoria donde se almacena la dirección IPv4 encontrada.
 *
 * VALOR DEVUELTO:
 *   Se devuelve 0 si la cadena de texto representaba una dirección IPv4.
 *
 * ERRORES:
 *   La función devuelve -1 si la cadena de texto no representaba una
 *   dirección IPv4.
 */
int ipv4_str_addr ( char* str, ipv4_addr_t addr )
{
  int err = -1;

  if (str != NULL) {
    unsigned int addr_int[IPv4_ADDR_SIZE];
    int len = sscanf(str, "%d.%d.%d.%d", 
                     &addr_int[0], &addr_int[1], 
                     &addr_int[2], &addr_int[3]);

    if (len == IPv4_ADDR_SIZE) {
      int i;
      for (i=0; i<IPv4_ADDR_SIZE; i++) {
        addr[i] = (unsigned char) addr_int[i];
      }
      
      err = 0;
    }
  }
  
  return err;
}


/*
 * uint16_t ipv4_checksum ( unsigned char * data, int len )
 *
 * DESCRIPCIÓN:
 *   Esta función calcula el checksum IP de los datos especificados.
 *
 * PARÁMETROS:
 *   'data': Puntero a los datos sobre los que se calcula el checksum.
 *    'len': Longitud en bytes de los datos.
 *
 * VALOR DEVUELTO:
 *   El valor del checksum calculado.
 */
uint16_t ipv4_checksum ( unsigned char * data, int len )
{
  int i;
  uint16_t word16;
  unsigned int sum = 0;
    
  /* Make 16 bit words out of every two adjacent 8 bit words in the packet
   * and add them up */
  for (i=0; i<len; i=i+2) {
    word16 = ((data[i] << 8) & 0xFF00) + (data[i+1] & 0x00FF);
    sum = sum + (unsigned int) word16;	
  }

  /* Take only 16 bits out of the 32 bit sum and add up the carries */
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  /* One's complement the result */
  sum = ~sum;

  return (uint16_t) sum;
}

int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol,  unsigned char * payload, int payload_len){
  ipv4_route_t *route = ipv4_route_table_lookup(layer->routing_table, dst);
  if (!route) {
    return -1;
  }


  mac_addr_t next_hop_mac;
  ipv4_addr_t next_hop_ip;

  if (memcmp(route->gateway_addr, IPv4_ZERO_ADDR, IPv4_ADDR_SIZE) == 0) {
    memcpy(next_hop_ip, dst, IPv4_ADDR_SIZE);
  } else {
    memcpy(next_hop_ip, route->gateway_addr, IPv4_ADDR_SIZE);
  }

  if (arp_resolve(layer->iface, next_hop_ip, next_hop_mac) != 0) {
    return -1;
  }

  int header_len = sizeof(ipv4_header_t);
  int total_len = header_len + payload_len;
  unsigned char* buffer = malloc(total_len);

  ipv4_header_t* ip_header = (ipv4_header_t*) buffer;
  ip_header->version_ihl = (4 << 4) | 5;
  ip_header->type_of_service = 0;
  ip_header->total_length = htons(total_len);
  ip_header->identification = 0;
  ip_header->flags_fragment_offset = 0;
  ip_header->time_to_live = 64;
  ip_header->protocol = protocol;
  ip_header->header_checksum = 0;
  memcpy(ip_header->src_addr, layer->addr, IPv4_ADDR_SIZE);
  memcpy(ip_header->dest_addr, dst, IPv4_ADDR_SIZE);

  uint16_t checksum = ipv4_checksum((unsigned char*)ip_header, header_len);
  ip_header->header_checksum = htons(checksum);

  memcpy(buffer + header_len, payload, payload_len);

  int bytes_sent = eth_send(layer->iface, next_hop_mac, ETH_TYPE_IPV4, buffer, total_len);

  free(buffer);

  return bytes_sent;
}

ipv4_layer_t * ipv4_open(char * file_conf, char * file_conf_route) {
  ipv4_layer_t * layer = malloc(sizeof(ipv4_layer_t));
  if (!layer) {
    perror("malloc ipv4_layer_t");
    return NULL;
  }

  /* 1. Crear layer->routing_table */
  layer->routing_table = ipv4_route_table_create();
  if (!layer->routing_table) {
    free(layer);
    return NULL;
  }

  /* 2. Leer direcciones y subred de file_conf */
  char ifname[IFACE_NAME_MAX_LENGTH];
  if (ipv4_config_read(file_conf, ifname, layer->addr, layer->netmask) != 0) {
    fprintf(stderr, "Error reading IPv4 config file %s\n", file_conf);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  /* 3. Leer tabla de reenvío IP de file_conf_route */
  if (ipv4_route_table_read(file_conf_route, layer->routing_table) < 0) {
    fprintf(stderr, "Error reading IPv4 route table file %s\n", file_conf_route);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  /* 4. Inicializar capa Ethernet con eth_open() */
  layer->iface = eth_open(ifname);
  if (!layer->iface) {
    fprintf(stderr, "Error opening Ethernet interface %s\n", ifname);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  return layer;
}

// Helper to print hex data
void print_hex(unsigned char *data, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol,
              unsigned char buffer[], ipv4_addr_t sender, int buf_len,
              long int timeout) {

    mac_addr_t src_mac;
    unsigned char eth_buffer[ETH_MTU];
    int payload_len;

    while (1) {
        payload_len = eth_recv(layer->iface, src_mac, ETH_TYPE_IPV4, eth_buffer, sizeof(eth_buffer), timeout);
        if (payload_len <= 0) {
            return -1; // Timeout or error
        }

        if (payload_len < sizeof(ipv4_header_t)) {
            // Packet too small to be a valid IPv4 packet
            continue;
        }

        ipv4_header_t *ip_header = (ipv4_header_t *)eth_buffer;

        // 1. Validate Version and Header Length
        if ((ip_header->version_ihl >> 4) != 4) {
            fprintf(stderr, "IPv4 Recv: Incorrect IP version\n");
            continue;
        }
        unsigned int header_len = (ip_header->version_ihl & 0x0F) * 4;
        if (header_len < sizeof(ipv4_header_t)) {
            fprintf(stderr, "IPv4 Recv: Invalid header length\n");
            continue;
        }

        // 2. Validate Checksum
        uint16_t received_checksum = ip_header->header_checksum;
        ip_header->header_checksum = 0;
        uint16_t calculated_checksum = ipv4_checksum((unsigned char *)ip_header, header_len);
        ip_header->header_checksum = received_checksum;

        if (received_checksum != htons(calculated_checksum)) {
            fprintf(stderr, "IPv4 Recv: Invalid checksum\n");
            continue;
        }

        // 3. Check destination address
        if (memcmp(ip_header->dest_addr, layer->addr, IPv4_ADDR_SIZE) != 0) {
            // Not for us
            continue;
        }

        // 4. Check protocol
        if (ip_header->protocol != protocol) {
            continue;
        }

        // 5. Get payload
        int ip_total_len = ntohs(ip_header->total_length);
        int ip_payload_len = ip_total_len - header_len;

        if (ip_payload_len <= 0) {
            continue;
        }

        // 6. Copy sender IP
        memcpy(sender, ip_header->src_addr, IPv4_ADDR_SIZE);

        // 7. Copy payload to user buffer
        int len_to_copy = (ip_payload_len > buf_len) ? buf_len : ip_payload_len;
        unsigned char *payload = eth_buffer + header_len;
        memcpy(buffer, payload, len_to_copy);

        // 8. Print payload to screen
        printf("Received IPv4 packet with protocol %d from ", protocol);
        char sender_str[IPv4_STR_MAX_LENGTH];
        ipv4_addr_str(sender, sender_str);
        printf("%s\n", sender_str);
        printf("Payload (%d bytes):\n", len_to_copy);
        print_hex(buffer, len_to_copy);

        return len_to_copy;
    }
}
