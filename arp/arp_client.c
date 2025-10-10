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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <target_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *interface = argv[1];
    char *target_ip_str = argv[2];

    mac_addr_t target_mac;

    ipv4_addr_t target_ip;


    if (ipv4_str_addr(target_ip_str, target_ip) != 0) {
        fprintf(stderr, "Invalid target IP address: %s\n", target_ip_str);
        exit(EXIT_FAILURE);
    }

    eth_iface_t *iface = eth_open(interface);
    if (iface == NULL) {
        fprintf(stderr, "Failed to open interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    if (arp_resolve(iface, target_ip, target_mac) == 1) {
        char mac_str[MAC_STR_LENGTH];
        mac_addr_str(target_mac, mac_str);
        printf("Resolved MAC address: %s\n", mac_str);
    } else {
        fprintf(stderr, "Could not resolve MAC address for %s\n", target_ip_str);
    }

    eth_close(iface);

    return 0;
}
