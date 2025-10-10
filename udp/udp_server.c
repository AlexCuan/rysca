#include "udp_server.h"
#include "udp.h"
#include <stdio.h>

int main() {
    udp_layer_t* udp_layer = udp_open("../configs/ipv4_config_server.txt", "../configs/ipv4_route_table_server.txt");
    if (!udp_layer) {
        perror("udp_open");
        return -1;
    }

    unsigned char buffer[1500];
    ipv4_addr_t src_addr;
    uint16_t src_port;

    printf("UDP server listening\n");

    int bytes_received = udp_rcv(udp_layer, &src_port, src_addr, buffer, 1500, -1);
    if (bytes_received < 0) {
        perror("udp_rcv");
        udp_close(udp_layer);
        return -1;
    }

    char src_addr_str[IPv4_STR_MAX_LENGTH];
    ipv4_addr_str(src_addr, src_addr_str);

    printf("Received %d bytes from %s:%d\n", bytes_received, src_addr_str, src_port);
    printf("Message: %.*s\n", bytes_received, buffer);

    udp_close(udp_layer);
    return 0;
}
