#include "ripv2.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * Cliente RIPv2 modificado para soportar peticiones específicas.
 * Uso: ./ripv2_client <conf> <routes> <server_ip> [subnet mask] [subnet mask] ...
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: ./ripv2_client <config_file> <routes_file> <server_ip> [subnet mask] ...\n");
        return -1;
    }
    char *config = argv[1];
    char *routes = argv[2];
    char *server_ip_str = argv[3];

    udp_layer_t *udp_layer = udp_open(config, routes, 0);
    if (!udp_layer) {
        perror("Failed to open UDP layer");
        return -1;
    }
    ipv4_addr_t dest_ip;
    if(ipv4_str_addr(server_ip_str, dest_ip) != 0) {
        fprintf(stderr, "ERROR: Invalid server IP address '%s'\n", server_ip_str);
        return -1;
    };

    ripv2_msg_t msg = {0};
    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;
    int payload_len = 0;

    // --- CONSTRUCCIÓN DEL REQUEST ---
    if (argc == 4) {
        // No hay argumentos extra -> Whole Table Request
        printf("Generating Whole Table Request...\n");
        msg.entries[0].family = 0;
        msg.entries[0].metric = htonl(16);
        payload_len = RIP_HEADER_SIZE + RIP_ENTRY_SIZE;
    } else {
        // Hay argumentos extra -> Specific Request
        printf("Generating Specific Request...\n");
        int entry_idx = 0;
        for (int i = 4; i < argc; i += 2) {
            if (entry_idx >= 25) break;
            if (i + 1 >= argc) {
                printf("Warning: Missing mask for subnet %s. Ignoring.\n", argv[i]);
                break;
            }
            char *sub_str = argv[i];
            char *mask_str = argv[i+1];

            msg.entries[entry_idx].family = htons(2); // AF_INET
            // El resto de campos (tag, next_hop) a 0 por defecto
            ipv4_str_addr(sub_str, msg.entries[entry_idx].ip);
            ipv4_str_addr(mask_str, msg.entries[entry_idx].mask);
            msg.entries[entry_idx].metric = htonl(16); // Se puede poner cualquier cosa, 16 es habitual

            entry_idx++;
        }
        payload_len = RIP_HEADER_SIZE + (entry_idx * RIP_ENTRY_SIZE);
    }

    udp_send(udp_layer, dest_ip, RIP_PORT, (unsigned char *)&msg, payload_len, 0);
    printf("RIPv2 Request sent to %s (%d bytes)\n", server_ip_str, payload_len);

    // --- RECEPCIÓN ---
    uint16_t src_port;
    ipv4_addr_t src_ip;
    unsigned char buffer[1500];

    int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, 1500, 5000);

    if (len > 0) {
        ripv2_msg_t *response = (ripv2_msg_t *)buffer;
        if (response->command == RIP_COMMAND_RESPONSE) {
            int num_entries = (len - RIP_HEADER_SIZE) / RIP_ENTRY_SIZE;
            printf("Received RIPv2 Response with %d entries:\n", num_entries);
            printf("------------------------------------------\n");

            for (int i = 0; i < num_entries; i++) {
                ripv2_entry_t *entry = &response->entries[i];
                char ip_str[16], mask_str[16];
                ipv4_addr_str(entry->ip, ip_str);
                ipv4_addr_str(entry->mask, mask_str);
                uint32_t metric = ntohl(entry->metric);

                printf("  Entry %d: Subnet %s/%s | Metric: %u\n",
                       i + 1, ip_str, mask_str, metric);
            }
            printf("------------------------------------------\n");
        }
    } else {
        printf("Failed to receive RIPv2 Response.\n");
    }

    udp_close(udp_layer);
    return 0;
}