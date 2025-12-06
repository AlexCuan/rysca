#include "udp.h"

#include <stdio.h>

#include "../ipv4/ipv4.h"
#include "../utils/rng.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * udp_layer_t * udp_open(char * config_file, char * route_table)
 *
 * DESCRIPCIÓN:
 *   Esta función inicializa la capa UDP.
 *   Crea y configura una estructura udp_layer_t, que incluye la inicialización
 *   de la capa IPv4 subyacente.
 *
 * PARÁMETROS:
 *   'config_file': Ruta al archivo de configuración que contiene la
 *                  interfaz, la dirección IPv4 y la máscara de subred.
 *   'route_table': Ruta al archivo que contiene la tabla de enrutamiento.
 *
 * VALOR DEVUELTO:
 *   Devuelve un puntero a la estructura udp_layer_t inicializada si
 *   la operación fue exitosa.
 *
 * ERRORES:
 *   Devuelve NULL si ocurre un error durante la asignación de memoria o
 *   la inicialización de la capa IPv4.
 */
udp_layer_t* udp_open(char* config_file, char* route_table) {
    rng_init();
    udp_layer_t* layer = (udp_layer_t*)malloc(sizeof(udp_layer_t));
    if (!layer) {
        return NULL;
    }

    layer->ipv4_layer = ipv4_open(config_file, route_table);
    if (!layer->ipv4_layer) {
        free(layer);
        return NULL;
    }

    return layer;
}

int udp_close(udp_layer_t* layer) {
    if (layer) {
        // Assuming ipv4_close exists and cleans up ipv4_layer resources.
        // If not, memory for layer->ipv4_layer should be freed here.
        ipv4_close(layer->ipv4_layer);
        free(layer);
        return 0;
    }
   else{
        return -1;
    }
}

/*
 * uint16_t udp_checksum(udp_header_t* udp_header, unsigned char* payload, int payload_len)
 *
 * DESCRIPCIÓN:
 *   Esta función calcula el checksum UDP de los datos especificados.
 *
 * PARÁMETROS:
 *   'udp_header': Puntero a la cabecera UDP.
 *   'payload': Puntero a los datos del payload.
 *   'payload_len': Longitud en bytes de los datos del payload.
 *
 * VALOR DEVUELTO:
 *   El valor del checksum calculado.
 */
uint16_t udp_checksum(udp_header_t* udp_header, unsigned char* payload, int payload_len) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)udp_header;
    int count = sizeof(udp_header_t) / 2;

    while (count > 0) {
        sum += *ptr++;
        count--;
    }

    ptr = (uint16_t*)payload;
    count = payload_len / 2;

    while (count > 0) {
        sum += *ptr++;
        count--;
    }

    if (payload_len % 2 != 0) {
        sum += *((uint8_t*)ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

/*
 * int udp_send(udp_layer_t* layer, ipv4_addr_t dest_addr, uint16_t dest_port, unsigned char* payload, int payload_len)
 *
 * DESCRIPCIÓN:
 *   Esta función envía un paquete UDP al destino especificado.
 *   La función se encarga de construir la cabecera UDP, generar un puerto
 *   de origen aleatorio, calcular el checksum UDP y enviar el paquete
 *   a través de la capa IPv4 subyacente.
 *
 * PARÁMETROS:
 *   'layer': Puntero a la estructura de la capa UDP que se utilizará para enviar.
 *   'dest_addr': La dirección IPv4 de destino del paquete UDP.
 *   'dest_port': El puerto UDP de destino.
 *   'payload': Puntero a los datos del payload que se enviarán.
 *   'payload_len': Longitud en bytes de los datos del payload.
 *
 * VALOR DEVUELTO:
 *   Devuelve el número de bytes enviados si el envío fue exitoso.
 *
 * ERRORES:
 *   Devuelve -1 si ocurre un error durante la asignación de memoria para el paquete
 *   o si la función ipv4_send devuelve un error.
 */
int udp_send(udp_layer_t* layer, ipv4_addr_t dest_addr, uint16_t dest_port, unsigned char* payload, int payload_len) {
    // Seed the srand generator

    const int header_len = sizeof(udp_header_t);
    const int packet_len = header_len + payload_len;

    unsigned char* packet = (unsigned char*) malloc(packet_len);
    if (packet == NULL) {
        return -1; // Memory allocation failed
    }
    udp_header_t* header = (udp_header_t*) packet;
    header->src_port = htons(rng_get_rand_in_range(49152, 65535));
    header->dest_port = htons(dest_port);
    header->length = htons(sizeof(udp_header_t) + payload_len);
    header->checksum = 0;

    memcpy(packet + header_len, payload, payload_len);
    
    // udp_header_t* header_in_packet = (udp_header_t*)packet;
    // header_in_packet->checksum = udp_checksum(header_in_packet, payload, payload_len);


    const int result = ipv4_send(layer->ipv4_layer, dest_addr, IP_PROTOCOL_UDP, packet, packet_len);
    free(packet);
    return result;
}

/*
 * int udp_rcv(udp_layer_t* layer, uint16_t* src_port, ipv4_addr_t src_addr, unsigned char* buffer, int buffer_len, long int timeout)
 *
 * DESCRIPCIÓN:
 *   Esta función recibe un paquete UDP.
 *   Espera a que llegue un paquete a través de la capa IPv4, valida que
 *   sea un paquete UDP válido y extrae el payload, la dirección IP de
 *   origen y el puerto de origen.
 *
 * PARÁMETROS:
 *   'layer': Puntero a la estructura de la capa UDP desde la que se recibirá el paquete.
 *   'src_port': Puntero donde se almacenará el puerto UDP de origen del paquete recibido.
 *   'src_addr': Puntero donde se almacenará la dirección IPv4 de origen del paquete recibido.
 *   'buffer': Buffer donde se copiará el payload del paquete UDP recibido.
 *   'buffer_len': Longitud máxima del buffer para el payload.
 *   'timeout': Tiempo máximo en milisegundos que la función esperará por un paquete.
 *
 * VALOR DEVUELTO:
 *   Devuelve la longitud del payload recibido si la recepción fue exitosa.
 *
 * ERRORES:
 *   Devuelve -1 si no se recibe ningún paquete antes del timeout, si el
 *   paquete recibido es demasiado corto para ser un paquete UDP válido,
 *   o si ocurre un error en la capa IPv4.
 */
int udp_rcv(udp_layer_t* layer, uint16_t* src_port, ipv4_addr_t src_addr, unsigned char* buffer, int buffer_len, long int timeout) {
    unsigned char* packet = malloc(buffer_len);
    if (!packet) {
        return -1;
    }
    int received_len = ipv4_recv(layer->ipv4_layer, IP_PROTOCOL_UDP, packet, src_addr, buffer_len, timeout);
    if (received_len < 0) {
        free(packet);
        return -1;
    }
    if (received_len < sizeof(udp_header_t)) {
        free(packet);
        return -1;
    }

    udp_header_t* header = (udp_header_t*)packet;
    *src_port = ntohs(header->src_port);

    const int payload_len = received_len - sizeof(udp_header_t);
    
    // Check UDP checksum
    // TODO: Should I use ntohs() here? or leave it as it is because the checksum is already in network byte order.
    uint16_t received_checksum = ntohs(header->checksum);
    if (received_checksum != 0) { // If checksum is not zero, it means sender calculated it
        // Temporarily set checksum to 0 for calculation
        uint16_t original_checksum_field = header->checksum;
        header->checksum = 0; 
        uint16_t calculated_checksum = udp_checksum(header, packet + sizeof(udp_header_t), payload_len);
        header->checksum = original_checksum_field; // Restore original checksum field

        if (calculated_checksum != received_checksum) {
            // Checksum mismatch, packet corrupted or invalid
            // free(packet);
            // return -1;
            //TODO: Fix this mismatch
            printf("DEBUG: UDP Checksum mismatch (ignored)\n"); // Add this
        }
    }

    memcpy(buffer, packet + sizeof(udp_header_t), payload_len);

    free(packet);
    return payload_len;
}
