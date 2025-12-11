#include "udp_client.h"
#include "udp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_usage(const char *prog_name) {
    printf("Usage: %s <ipv4_config_file> <ipv4_route_table_file> <dest_ip> <message>\n", prog_name);
    printf("Or: %s -h\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc != 5) {
        fprintf(stderr, "Error: Invalid arguments\n");
        print_usage(argv[0]);
        return -1;
    }

    char *config_file = argv[1];
    char *route_table_file = argv[2];
    char *dest_ip_str = argv[3];
    char *message = argv[4];

    udp_layer_t* udp_layer = udp_open(config_file, route_table_file);
    if (!udp_layer) {
        perror("udp_open");
        return -1;
    }

    ipv4_addr_t dest_addr;
    if (ipv4_str_addr(dest_ip_str, dest_addr) != 0) {
        fprintf(stderr, "Error: Invalid destination IP address\n");
        udp_close(udp_layer);
        return -1;
    }

    int message_len = strlen(message);

    int bytes_sent = udp_send(udp_layer, 0, dest_addr, UDP_PORT_SERVER, (unsigned char*)message, message_len);
    if (bytes_sent < 0) {
        perror("udp_send");
        udp_close(udp_layer);
        return -1;
    }

    printf("Sent %d bytes in total to %s:%d\n", bytes_sent, dest_ip_str, UDP_PORT_SERVER);

    udp_close(udp_layer);
    return 0;
}
