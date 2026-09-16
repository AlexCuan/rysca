#include "udp_server.h"
#include "udp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Required for atoi

void print_usage(const char* prog_name)
{
    printf("Usage: %s <ipv4_config_file> <ipv4_route_table_file> <listening_port>\n", prog_name);
    printf("Or: %s -h\n", prog_name);
}

int main(int argc, char* argv[])
{
    if (argc == 2 && strcmp(argv[1], "-h") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }

    // Expect 3 arguments now: config, route_table, and port
    if (argc != 4)
    {
        fprintf(stderr, "Error: Invalid arguments\n");
        print_usage(argv[0]);
        return -1;
    }

    char* config_file = argv[1];
    char* route_table_file = argv[2];
    uint16_t server_port = (uint16_t)atoi(argv[3]); // Parse port from argument

    udp_layer_t* udp_layer = udp_open(config_file, route_table_file, server_port);
    if (!udp_layer)
    {
        perror("udp_open");
        return -1;
    }

    unsigned char buffer[1500];
    ipv4_addr_t src_addr;
    uint16_t src_port;

    printf("UDP server listening on port %d\n", server_port);

    while (1)
    {
        int bytes_received = udp_rcv(udp_layer, &src_port, src_addr, buffer, 1500, -1);
        if (bytes_received == 0)
        {
            continue; // Timeout
        }
        if (bytes_received < 0)
        {
            perror("udp_rcv");
            continue;
        }

        char src_addr_str[IPv4_STR_MAX_LENGTH];
        ipv4_addr_str(src_addr, src_addr_str);

        printf("Received %d bytes (of UDP payload) from %s:%d\n", bytes_received, src_addr_str, src_port);
        printf("Message: %.*s\n", bytes_received, buffer);
    }

    udp_close(udp_layer);
    return 0;
}
