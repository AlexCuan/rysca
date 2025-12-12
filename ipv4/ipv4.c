
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

/*
 * int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol, unsigned char * payload, int payload_len)
 *
 * DESCRIPCIÓN:
 *   Esta función envía un paquete IPv4 al destino especificado.
 *   La función se encarga de construir la cabecera IPv4, calcular el checksum
 *   y enviar el paquete a través de la capa Ethernet.
 *   Busca la ruta adecuada en la tabla de enrutamiento para determinar
 *   la siguiente dirección IP de salto. Luego, resuelve la dirección MAC
 *   del siguiente salto usando ARP antes de enviar el paquete.
 *
 * PARÁMETROS:
 *   'layer': Puntero a la estructura de la capa IPv4 que se utilizará para enviar.
 *     'dst': La dirección IPv4 de destino del paquete.
 * 'protocol': El protocolo de la capa superior de los datos del payload (por ejemplo, TCP, UDP).
 *  'payload': Puntero a los datos del payload que se enviarán.
 *'payload_len': Longitud en bytes de los datos del payload.
 *
 * VALOR DEVUELTO:
 *   Devuelve el número de bytes enviados si el envío fue exitoso.
 *
 * ERRORES:
 *   Devuelve -1 si no se encuentra una ruta al destino, si la resolución
 *   ARP falla, o si ocurre un error en la capa Ethernet.
 */
int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol,  unsigned char * payload, int payload_len, int corrupt){
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

  if (arp_resolve(layer->iface, layer->addr, next_hop_ip, NULL, next_hop_mac) != 0) {
    return -1;
  }

  const int header_len = sizeof(ipv4_header_t);
  const int total_len = header_len + payload_len;
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

  if (corrupt) {
    checksum ^= 0xFFFF; // Corrupt the checksum
    printf("DEBUG: Corrupting IPv4 Checksum\n");
  }

  ip_header->header_checksum = htons(checksum);

  memcpy(buffer + header_len, payload, payload_len);

  int bytes_sent = eth_send(layer->iface, next_hop_mac, ETH_TYPE_IPV4, buffer, total_len);

  free(buffer);

  return bytes_sent;
}

/*
 * ipv4_layer_t * ipv4_open(char * file_conf, char * file_conf_route)
 *
 * DESCRIPCIÓN:
 *   Esta función inicializa la capa IPv4.
 *   Crea y configura una estructura ipv4_layer_t, que incluye la creación
 *   de una tabla de enrutamiento, la lectura de la configuración de red
 *   (dirección IP, máscara de subred) desde un archivo y la carga de la
 *   tabla de enrutamiento desde otro archivo. Finalmente, inicializa la
 *   capa Ethernet subyacente.
 *
 * PARÁMETROS:
 *   'file_conf': Ruta al archivo de configuración que contiene la
 *                interfaz, la dirección IPv4 y la máscara de subred.
 *   'file_conf_route': Ruta al archivo que contiene la tabla de enrutamiento.
 *
 * VALOR DEVUELTO:
 *   Devuelve un puntero a la estructura ipv4_layer_t inicializada si
 *   la operación fue exitosa.
 *
 * ERRORES:
 *   Devuelve NULL si ocurre un error durante la asignación de memoria,
 *   la lectura de los archivos de configuración o la inicialización
 *   de la capa Ethernet.
 */
ipv4_layer_t * ipv4_open(char * file_conf, char * file_conf_route) {
  ipv4_layer_t * layer = malloc(sizeof(ipv4_layer_t));
  if (!layer) {
    perror("malloc ipv4_layer_t");
    return NULL;
  }

  //Crear routing_table
  layer->routing_table = ipv4_route_table_create();
  if (!layer->routing_table) {
    free(layer);
    return NULL;
  }

  // Leer direcciones y subred de file_conf
  char ifname[IFACE_NAME_MAX_LENGTH];
  if (ipv4_config_read(file_conf, ifname, layer->addr, layer->netmask) != 0) {
    fprintf(stderr, "Error reading IPv4 config file %s\n", file_conf);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  // Leer tabla de reenvío IP de file_conf_route
  if (ipv4_route_table_read(file_conf_route, layer->routing_table) < 0) {
    fprintf(stderr, "Error reading IPv4 route table file %s\n", file_conf_route);
    ipv4_route_table_free(layer->routing_table);
    free(layer);
    return NULL;
  }

  // Inicializar capa Ethernet con eth_open()
  layer->iface = eth_open(ifname);
  if (!layer->iface) {
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
 * DESCRIPCIÓN:
 *   Esta función libera los recursos asociados a una capa IPv4.
 *   Cierra la interfaz Ethernet, libera la tabla de enrutamiento y
 *   libera la memoria de la estructura ipv4_layer_t.
 *
 * PARÁMETROS:
 *   'layer': Puntero a la estructura de la capa IPv4 a cerrar.
 *
 * VALOR DEVUELTO:
 *   Devuelve 0 si la operación fue exitosa.
 *
 * ERRORES:
 *   Devuelve -1 si el puntero 'layer' es NULL.
 */
int ipv4_close(ipv4_layer_t * layer) {
  if (!layer) {
    fprintf(stderr, "ipv4_close: 'layer' cannot be NULL\n");
    return -1;
  }

  if (layer->iface) {
    eth_close(layer->iface);
  }
  if (layer->routing_table) {
    ipv4_route_table_free(layer->routing_table);
  }
  free(layer);

  return 0;
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

/*
 * int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol, unsigned char buffer[], ipv4_addr_t sender, int buf_len, long int timeout)
 *
 * DESCRIPCIÓN:
 *   Esta función se encarga de recibir paquetes IPv4. Espera la llegada de
 *   un paquete IPv4 en la interfaz de red asociada a la capa IPv4.
 *   Realiza varias validaciones sobre el paquete recibido, incluyendo la
 *   versión IP, la longitud de la cabecera, el checksum y la dirección de
 *   destino. Si el paquete es válido y está destinado a esta interfaz y
 *   protocolo, copia el payload a un buffer proporcionado por el usuario.
 *
 * PARÁMETROS:
 *   'layer': Puntero a la estructura de la capa IPv4 donde se recibirá el paquete.
 * 'protocol': El protocolo de la capa superior esperado (por ejemplo, TCP, UDP).
 *  'buffer': Buffer donde se copiará el payload del paquete recibido.
 *  'sender': Array donde se almacenará la dirección IPv4 del remitente del paquete.
 * 'buf_len': Longitud máxima del buffer proporcionado para el payload.
 * 'timeout': Tiempo máximo en milisegundos que la función esperará por un paquete.
 *
 * VALOR DEVUELTO:
 *   Devuelve la longitud del payload recibido si el paquete fue procesado
 *   exitosamente.
 *
 * ERRORES:
 *   Devuelve -1 si ocurre un error en la capa Ethernet
 */
int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol,
              unsigned char buffer[], ipv4_addr_t sender, int buf_len,
              long int timeout) {

    mac_addr_t src_mac;
    unsigned char eth_buffer[ETH_MTU];
    int payload_len;

    while (1) {
        payload_len = eth_recv(layer->iface, src_mac, ETH_TYPE_IPV4, eth_buffer, sizeof(eth_buffer), timeout);
        if (payload_len <= 0) {
            return -1; // Error
        }


        if (payload_len < sizeof(ipv4_header_t)) {
            // Packet too small to be a valid IPv4 packet
            continue;
        }
        // TODO: Check this redundant cast
        ipv4_header_t *ip_header = (ipv4_header_t *)eth_buffer;

        // Validate Version and Header Length
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

        if (ip_header->protocol != protocol) {
          // printf("Ignored packet with protocol %d (Expected %d)\n", ip_header->protocol, protocol);
          continue;
      }

        // 3. Check destination address
        int is_for_me = (memcmp(ip_header->dest_addr, layer->addr, IPv4_ADDR_SIZE) == 0);

        // CORRECCIÓN MULTICAST: 224.0.0.0/4 (0xE0...)
        int is_multicast = ((ip_header->dest_addr[0] & 0xF0) == 0xE0);
        int is_broadcast = (ip_header->dest_addr[3] == 255); // Simplificación broadcast

        // --- DIAGNÓSTICO ---
        printf("[IPv4 DEBUG] Paquete recibido para %d.%d.%d.%d (Mio:%d, Multi:%d)\n",
               ip_header->dest_addr[0], ip_header->dest_addr[1],
               ip_header->dest_addr[2], ip_header->dest_addr[3],
               is_for_me, is_multicast);

        if (!is_for_me && !is_multicast && !is_broadcast) {
             printf("[IPv4 DEBUG] ... Descartado por IP destino incorrecta.\n");
             continue; // No es para nosotros
        }

        // 5. Get payload
        const int ip_total_len = ntohs(ip_header->total_length);
        const int ip_payload_len = ip_total_len - header_len;

        if (ip_payload_len <= 0) {
            continue;
        }

        // 6. Copy sender IP
        memcpy(sender, ip_header->src_addr, IPv4_ADDR_SIZE);

        // 7. Copy payload to user buffer
        //Ternario: (condición) ? (valor_si_verdadero) : (valor_si_falso);
        // Prevent buffer overflow
        const int len_to_copy = (ip_payload_len > buf_len) ? buf_len : ip_payload_len;
        unsigned char *payload = eth_buffer + header_len;
        memcpy(buffer, payload, len_to_copy);

        // 8. Print payload to screen
        printf("Received IPv4 packet with protocol %d from ", protocol);
        // char sender_str[IPv4_STR_MAX_LENGTH];
        // ipv4_addr_str(sender, sender_str);
        // printf("%s\n", sender_str);
        // printf("IP Payload (%d bytes):\n", len_to_copy);
        // print_hex(buffer, len_to_copy);

        return len_to_copy;
    }
}
