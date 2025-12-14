#include "udp_client.h"
#include "udp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_usage(const char* prog_name)
{
    printf("Usage: %s <ipv4_config_file> <ipv4_route_table_file> <dest_ip> <dest_port> <message> [-e]\n", prog_name);
    printf("Or: %s -h\n", prog_name);
}

int main(int argc, char* argv[])
{
    if (argc == 2 && strcmp(argv[1], "-h") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }

    int corrupt = 0;

    // Expect 6 arguments normally (prog + 5 args)
    // Expect 7 arguments if -e is present
    if (argc == 7 && strcmp(argv[6], "-e") == 0)
    {
        corrupt = 1;
    }
    else if (argc != 6)
    {
        fprintf(stderr, "Error: Invalid arguments\n");
        print_usage(argv[0]);
        return -1;
    }

    char* config_file = argv[1];
    char* route_table_file = argv[2];
    char* dest_ip_str = argv[3];
    uint16_t dest_port = (uint16_t)atoi(argv[4]); // Parse destination port
    char* message = argv[5];

    // Open UDP layer with port 0 (Random Source Port)
    udp_layer_t* udp_layer = udp_open(config_file, route_table_file, 0);
    if (!udp_layer)
    {
        perror("udp_open");
        return -1;
    }

    ipv4_addr_t dest_addr;
    if (ipv4_str_addr(dest_ip_str, dest_addr) != 0)
    {
        fprintf(stderr, "Error: Invalid destination IP address\n");
        udp_close(udp_layer);
        return -1;
    }

    int message_len = strlen(message);

    printf("Sending to %s:%d (Corrupt=%d)...\n", dest_ip_str, dest_port, corrupt);


    int bytes_sent = udp_send(udp_layer, dest_addr, dest_port, (unsigned char*)message, message_len, corrupt);
    if (bytes_sent < 0)
    {
        perror("udp_send");
        udp_close(udp_layer);
        return -1;
    }

    printf("Sent %d bytes in total to %s:%d\n", bytes_sent, dest_ip_str, dest_port);

    udp_close(udp_layer);
    return 0;
}
