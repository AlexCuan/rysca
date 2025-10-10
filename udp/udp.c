#include "udp.h"
#include "../utils/rng.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

udp_layer_t* udp_open(char* config_file, char* route_table) {
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
        free(layer);
    }
    return 0;
}

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


int udp_send(udp_layer_t* layer, ipv4_addr_t dest_addr, uint16_t dest_port, unsigned char* payload, int payload_len) {
    udp_header_t header;
    rng_init();
    header.src_port = htons(rng_get_rand_in_range(49152, 65535));
    header.dest_port = htons(dest_port);
    header.length = htons(sizeof(udp_header_t) + payload_len);
    header.checksum = 0;

    int packet_len = sizeof(udp_header_t) + payload_len;
    unsigned char* packet = (unsigned char*)malloc(packet_len);
    if (!packet) {
        return -1;
    }

    memcpy(packet, &header, sizeof(udp_header_t));
    memcpy(packet + sizeof(udp_header_t), payload, payload_len);
    
    udp_header_t* header_in_packet = (udp_header_t*)packet;
    header_in_packet->checksum = udp_checksum(header_in_packet, payload, payload_len);


    int result = ipv4_send(layer->ipv4_layer, dest_addr, IP_PROTOCOL_UDP, packet, packet_len);
    free(packet);
    return result;
}

int udp_rcv(udp_layer_t* layer, uint16_t* src_port, ipv4_addr_t src_addr, unsigned char* buffer, int buffer_len, long int timeout) {
    unsigned char packet[buffer_len];
    int received_len = ipv4_recv(layer->ipv4_layer, IP_PROTOCOL_UDP, packet, src_addr, buffer_len, timeout);

    if (received_len < sizeof(udp_header_t)) {
        return -1;
    }

    udp_header_t* header = (udp_header_t*)packet;
    *src_port = ntohs(header->src_port);

    int payload_len = received_len - sizeof(udp_header_t);
    memcpy(buffer, packet + sizeof(udp_header_t), payload_len);

    return payload_len;
}
