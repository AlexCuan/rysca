#include "ripv2.h"
#include "../udp/udp.h"
#include <stdio.h>
#include "ripv2_route_table.h"
#include <arpa/inet.h>
#include <string.h> // Needed for strcmp

#define RX_TIMEOUT_MS 2000

int main(int argc, char* argv[])
{
    int disable_checksum = 0;
    if (argc > 1 && strcmp(argv[argc - 1], "-d") == 0)
    {
        disable_checksum = 1;
        argc--; // The program now "believes" there is one argument less
    }

    if (argc < 4)
    {
        // Updated usage message
printf("Usage: ./ripv2_client <config_file> <routes_file> <server_ip> [subnet mask] ... [-d]\n");
        return -1;
    }


    char* config = argv[1];
    char* routes = argv[2];
    char* server_ip_str = argv[3];

    udp_layer_t* udp_layer = udp_open(config, routes, 0);
    if (!udp_layer)
    {
        perror("Failed to open UDP layer");
        return -1;
    }
    if (disable_checksum)
    {
        udp_layer->check_checksum = 0;
        printf(">>> AVISO: Verificación de Checksum UDP DESACTIVADA (flag -d) <<<\n");
    }

    // ------------------------------------

    ipv4_addr_t dest_ip;
    if (ipv4_str_addr(server_ip_str, dest_ip) != 0)
    {
        fprintf(stderr, "ERROR: Invalid server IP address '%s'\n", server_ip_str);
        return -1;
    };

    ripv2_msg_t msg = {0};
    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;
    int payload_len = 0;

    ripv2_route_table_t* client_table = ripv2_route_table_create();

    // --- REQUEST CONSTRUCTION ---
    if (argc == 4)
    {
        // No extra arguments -> whole table request
        printf("Generating Whole Table Request...\n");
        msg.entries[0].family = 0;
        msg.entries[0].metric = htonl(16);
        payload_len = RIP_HEADER_SIZE + RIP_ENTRY_SIZE;
    }
    else
    {
        // Extra arguments present -> specific request
        printf("Generating Specific Request...\n");
        int entry_idx = 0;
        for (int i = 4; i < argc; i += 2)
        {
            if (entry_idx >= 25) break;
            if (i + 1 >= argc)
            {
                printf("Warning: Missing mask for subnet %s. Ignoring.\n", argv[i]);
                break;
            }
            char* sub_str = argv[i];
            char* mask_str = argv[i + 1];

            msg.entries[entry_idx].family = htons(2); // AF_INET
            // The remaining fields (tag, next_hop) default to 0
            ipv4_str_addr(sub_str, msg.entries[entry_idx].ip);
            ipv4_str_addr(mask_str, msg.entries[entry_idx].mask);
            msg.entries[entry_idx].metric = htonl(16); // Any value works, 16 is customary

            entry_idx++;
        }
        payload_len = RIP_HEADER_SIZE + (entry_idx * RIP_ENTRY_SIZE);
    }

    udp_send(udp_layer, dest_ip, RIP_PORT, (unsigned char*)&msg, payload_len, 0);
    printf("RIPv2 Request sent to %s (%d bytes)\n", server_ip_str, payload_len);

    // --- RECEPTION ---
    printf("\nWaiting for response(s)...\n");

    uint16_t src_port;
    ipv4_addr_t src_ip;
    unsigned char buffer[1500];
    int packets_received = 0;

    while (1)
    {
        int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, sizeof(buffer), RX_TIMEOUT_MS);

        if (len <= 0)
        {
            // Timeout or error -> end of transmission
            break;
        }

        if (len < RIP_HEADER_SIZE) continue; // Incomplete header

        ripv2_msg_t* response = (ripv2_msg_t*)buffer;
        if (response->version != 2) continue;
        if (response->command != RIP_COMMAND_RESPONSE) continue;

        packets_received++;
        printf("Received packet #%d from server.\n", packets_received);

        // REUSE: the shared function is what fills the table
        ripv2_process_response(client_table, response, len, src_ip);
    }

    if (packets_received > 0)
    {
        printf("\n--- Final Consolidated Routing Table ---\n");
        ripv2_route_table_print(client_table); // Print the consolidated table
    }
    else
    {
        printf("Timeout: No response received.\n");
    }

    // Cleanup
    ripv2_route_table_free(client_table);
    udp_close(udp_layer);
    return 0;
}
