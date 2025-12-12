#include "../eth/eth.h"
#include "../ipv4/ipv4.h"
#include <rawnet.h>
#include <timerms.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "arp.h"

int main(const int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s <interface> <src_ip> <target_ip> [<target_mac>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *interface = argv[1];
    char *src_ip_str = argv[2];
    char *target_ip_str = argv[3];

    mac_addr_t mac;
    mac_addr_t target_mac_addr;
    unsigned char *target_mac_ptr = NULL;

    if (argc == 5) {
        if (mac_str_addr(argv[4], target_mac_addr) != 0) {
            fprintf(stderr, "Invalid target MAC address: %s\n", argv[4]);
            exit(EXIT_FAILURE);
        }
        target_mac_ptr = target_mac_addr;
    }

    ipv4_addr_t src_ip;
    ipv4_addr_t target_ip;

    if (ipv4_str_addr(src_ip_str, src_ip) != 0) {
        fprintf(stderr, "Invalid source IP address: %s\n", src_ip_str);
        exit(EXIT_FAILURE);
    }

    if (ipv4_str_addr(target_ip_str, target_ip) != 0) {
        fprintf(stderr, "Invalid target IP address: %s\n", target_ip_str);
        exit(EXIT_FAILURE);
    }

    eth_iface_t *iface = eth_open(interface);
    if (iface == NULL) {
        fprintf(stderr, "Failed to open interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    int result = arp_resolve(iface, src_ip, target_ip, target_mac_ptr, mac);
    if (result == 0) {
        char mac_str[MAC_STR_LENGTH];
        mac_addr_str(mac, mac_str);
        printf("Resolved MAC address: %s\n", mac_str);
    } else if (result == -1) {
        fprintf(stderr, "Could not resolve MAC address for %s\n", target_ip_str);
    } else if (result == -2) {
        fprintf(stderr, "Timeout error\n");
    }

    eth_close(iface);

    return 0;
}
