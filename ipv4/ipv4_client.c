#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ipv4.h"

#define PROTOCOL 123

int main(const int argc, char *argv[]) {
    // Check for optional -e flag
    int corrupt = 0;
    if (argc == 2 && strcmp(argv[1], "-e") == 0) {
        // Usage error
    } else if (argc > 1 && strcmp(argv[argc-1], "-e") == 0) {
        corrupt = 1;
    }

    char *config_file = "../configs/ipv4_config_client.txt";
    char *route_table_file = "../configs/ipv4_route_table_client.txt";
    char *server_ip_str = "192.100.101.101";
    char *message = "Hello from the client!";

    ipv4_layer_t *layer = ipv4_open(config_file, route_table_file);
    if (!layer) {
        printf("Error opening IPv4 layer\n");
        return 1;
    }

    ipv4_addr_t dest_addr;
    if (ipv4_str_addr(server_ip_str, dest_addr) != 0) {
        printf("Invalid destination IP address\n");
        ipv4_close(layer);
        return 1;
    }

    printf("Sending packet to %s... (Corrupt=%d)\n", server_ip_str, corrupt);

    int bytes_sent = ipv4_send(layer, dest_addr, PROTOCOL, (unsigned char *)message, strlen(message), corrupt);

    if (bytes_sent > 0) {
        printf("Packet sent successfully! (%d bytes)\n", bytes_sent);
    } else {
        printf("Error sending packet. ipv4_send returned: %d\n", bytes_sent);
    }

    ipv4_close(layer);
    return 0;
}