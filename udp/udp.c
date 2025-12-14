#include "udp.h"

#include <stdio.h>

#include "../ipv4/ipv4.h"
#include "../utils/rng.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

udp_layer_t* udp_open(char* config_file, char* route_table, uint16_t port)
{
    rng_init();
    udp_layer_t* layer = (udp_layer_t*)malloc(sizeof(udp_layer_t));
    if (!layer)
    {
        return NULL;
    }

    layer->ipv4_layer = ipv4_open(config_file, route_table);
    if (!layer->ipv4_layer)
    {
        free(layer);
        return NULL;
    }

    if (port == 0)
    {
        // Generar puerto aleatorio (Rango efímero IANA)
        layer->local_port = (uint16_t)rng_get_rand_in_range(49152, 65535);
    }
    else
    {
        layer->local_port = port;
    }
    layer->check_checksum = 1;
    printf("[UDP DEBUG]: UDP Layer opened on port %d\n", layer->local_port);

    return layer;
}

int udp_close(udp_layer_t* layer)
{
    if (layer)
    {
        ipv4_close(layer->ipv4_layer);
        free(layer);
        return 0;
    }
    else
    {
        return -1;
    }
}

uint16_t udp_checksum(ipv4_addr_t src, ipv4_addr_t dest, udp_header_t* udp_header, unsigned char* payload,
                      int payload_len)
{
    uint32_t sum = 0;
    uint16_t word16;

    // --- Pseudo Header ---
    // Source IP
    for (int i = 0; i < 4; i += 2)
    {
        sum += ((src[i] << 8) & 0xFF00) + (src[i + 1] & 0x00FF);
    }
    // Dest IP
    for (int i = 0; i < 4; i += 2)
    {
        sum += ((dest[i] << 8) & 0xFF00) + (dest[i + 1] & 0x00FF);
    }
    // Protocol (0 + 17) -> 0x0011
    sum += 0x0011;

    // UDP Length (Value from header)
    sum += ntohs(udp_header->length);


    // --- UDP Header ---
    unsigned char* h = (unsigned char*)udp_header;
    for (int i = 0; i < sizeof(udp_header_t); i += 2)
    {
        sum += ((h[i] << 8) & 0xFF00) + (h[i + 1] & 0x00FF);
    }

    // --- Payload ---
    for (int i = 0; i < payload_len; i += 2)
    {
        word16 = ((payload[i] << 8) & 0xFF00);
        if (i + 1 < payload_len)
        {
            word16 += (payload[i + 1] & 0x00FF);
        }
        sum += word16;
    }

    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}


int udp_send(udp_layer_t* layer, ipv4_addr_t dest_addr, uint16_t dest_port, unsigned char* payload, int payload_len,
             int corrupt)
{
    const int header_len = sizeof(udp_header_t);
    const int packet_len = header_len + payload_len;

    unsigned char* packet = (unsigned char*)malloc(packet_len);
    if (packet == NULL)
    {
        return -1;
    }
    udp_header_t* header = (udp_header_t*)packet;

    // USAR PUERTO LOCAL ALMACENADO
    header->src_port = htons(layer->local_port);
    header->dest_port = htons(dest_port);
    header->length = htons(sizeof(udp_header_t) + payload_len);
    header->checksum = 0;

    memcpy(packet + header_len, payload, payload_len);

    ipv4_addr_t src_addr;
    memcpy(src_addr, layer->ipv4_layer->addr, IPv4_ADDR_SIZE);

    uint16_t chk = udp_checksum(src_addr, dest_addr, header, payload, payload_len);
    if (chk == 0) chk = 0xFFFF;

    if (corrupt)
    {
        chk ^= 0xFFFF;
        printf("[UDP DEBUG]: Corrupting UDP Checksum\n");
    }

    header->checksum = htons(chk);

    const int result = ipv4_send(layer->ipv4_layer, dest_addr, IP_PROTOCOL_UDP, packet, packet_len, 0);
    free(packet);
    return result;
}


int udp_rcv(udp_layer_t* layer, uint16_t* src_port, ipv4_addr_t src_addr, unsigned char* buffer, int buffer_len,
            long int timeout)
{
    unsigned char* packet = malloc(buffer_len);
    if (!packet) return -1;
    ipv4_addr_t dest_addr_pkt; // Aquí se guardará la IP destino (Unicast o Multicast)

    while (1)
    {
        // ipv4_recv rellenará dest_addr_pkt con la IP destino del paquete recibido
        int received_len = ipv4_recv(layer->ipv4_layer, IP_PROTOCOL_UDP, packet, src_addr, dest_addr_pkt, buffer_len,
                                     timeout);

        if (received_len < 0)
        {
            free(packet);
            return -1;
        }
        if (received_len < sizeof(udp_header_t))
        {
            continue; // Paquete muy corto
        }

        udp_header_t* header = (udp_header_t*)packet;
        uint16_t dest_port_pkt = ntohs(header->dest_port);

        // Filtrar por puerto
        if (dest_port_pkt != layer->local_port)
        {
            continue;
        }

        *src_port = ntohs(header->src_port);
        const int payload_len = received_len - sizeof(udp_header_t);

        // --- VERIFICACIÓN DE CHECKSUM ---
        if (layer->check_checksum)
        {
            uint16_t received_checksum = ntohs(header->checksum);

            if (received_checksum != 0)
            {
                header->checksum = 0;
                // Recuerda usar dest_addr_pkt para que RIP multicast funcione
                uint16_t calculated_checksum = udp_checksum(src_addr, dest_addr_pkt, header,
                                                            packet + sizeof(udp_header_t), payload_len);
                if (calculated_checksum == 0) calculated_checksum = 0xFFFF;
                header->checksum = htons(received_checksum);

                if (calculated_checksum != received_checksum)
                {
                    printf("[UDP DEBUG] Error: UDP Checksum mismatch. Recv: 0x%04x, Calc: 0x%04x\n", received_checksum,
                           calculated_checksum);
                    continue; // DESCARTAR
                }
            }
        }

        // Si llegamos aquí, el checksum es válido o era 0.
        if (payload_len > 0)
        {
            memcpy(buffer, packet + sizeof(udp_header_t), payload_len);
        }

        free(packet);
        return payload_len;
    }
}
