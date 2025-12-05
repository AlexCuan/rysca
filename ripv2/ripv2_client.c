#include "ripv2.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>


int main(int argc, char *argv[]) {
    // 1. Argument parsing (simplified)
    char *config = argv[1];
    char *routes = argv[2];
    char *server_ip_str = argv[3];

    // 2. Open UDP Layer
    udp_layer_t *udp_layer = udp_open(config, routes);
    if (!udp_layer) { /* Handle Error */ }

    ripv2_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;

    // Special request for whole table: Family 0, Metric 16
    msg.entries[0].family = 0;
    msg.entries[0].metric = htonl(16); // Important: Network Byte Order

    // Size of packet = Header (4 bytes) + 1 Entry (20 bytes)
    int payload_len = 4 + 20;

    ipv4_addr_t dest_ip;
    ipv4_str_addr(server_ip_str, dest_ip);

    // Send to port 520
    udp_send(udp_layer, dest_ip, RIP_PORT, (unsigned char *)&msg, payload_len);
    printf("RIPv2 Request sent to %s\n", server_ip_str);

    uint16_t src_port;
    ipv4_addr_t src_ip;
    unsigned char buffer[1500]; // Buffer for response

    // Wait for response
    int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, 1500, 5000);

    if (len > 0) {
        ripv2_msg_t *response = (ripv2_msg_t *)buffer;

        if (response->command == RIP_COMMAND_RESPONSE) {
            int num_entries = (len - 4) / 20; // Header is 4 bytes, Entry is 20

            printf("Received RIPv2 Response with %d entries:\n", num_entries);

            for (int i = 0; i < num_entries; i++) {
                ripv2_entry_t *entry = &response->entries[i];

                char ip_str[16], mask_str[16];
                ipv4_addr_str(entry->ip, ip_str);
                ipv4_addr_str(entry->mask, mask_str);
                uint32_t metric = ntohl(entry->metric);

                printf("Route: %s/%s Metric: %d\n", ip_str, mask_str, metric);
            }
        }
    }
}