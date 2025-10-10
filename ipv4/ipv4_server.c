#include <stdio.h>
#include "ipv4.h"

#define PROTOCOL 123

int main() {
    char *config_file = "../configs/ipv4_config_server.txt";
    char *route_table_file = "../configs/ipv4_route_table_server.txt";

    ipv4_layer_t *layer = ipv4_open(config_file, route_table_file);
    if (!layer) {
        printf("Error opening IPv4 layer\n");
        return 1;
    }

    unsigned char buffer[1500];
    ipv4_addr_t sender;
    char sender_str[IPv4_STR_MAX_LENGTH];

    printf("Waiting for a packet...\n");
    int len = ipv4_recv(layer, PROTOCOL, buffer, sender, sizeof(buffer), -1);

    if (len > 0) {
        ipv4_addr_str(sender, sender_str);
        printf("Packet received from %s\n", sender_str);
        printf("Payload: %.*s\n", len, buffer);
    }

    return 0;
}
