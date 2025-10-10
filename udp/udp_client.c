#include "udp_client.h"
#include "udp.h"
#include <stdio.h>
#include <string.h>

int main() {
    udp_layer_t* udp_layer = udp_open("../configs/ipv4_config_client.txt", "../configs/ipv4_route_table_client.txt");
    if (!udp_layer) {
        perror("udp_open");
        return -1;
    }

    ipv4_addr_t dest_addr;
    ipv4_str_addr("192.100.101.101", dest_addr);

    char* message = "Hello, UDP Server!";
    int message_len = strlen(message);

    int bytes_sent = udp_send(udp_layer, dest_addr, UDP_PORT_SERVER, (unsigned char*)message, message_len);
    if (bytes_sent < 0) {
        perror("udp_send");
        udp_close(udp_layer);
        return -1;
    }

    printf("Sent %d bytes to %s:%d\n", bytes_sent, "192.100.101.101", UDP_PORT_SERVER);

    udp_close(udp_layer);
    return 0;
}
