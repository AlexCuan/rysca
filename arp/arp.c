//
// Created by Lucia Garcia on 17/9/25.
//

#include "arp.h"
#include "../eth/eth.h"
#include "../ipv4/ipv4.h"
#include <rawnet.h>
#include <timerms.h>
#include <netinet/in.h>
#include <string.h>

struct arp_pkt {
    uint16_t htype; // Hardware type
    uint16_t ptype; // Protocol type
    uint8_t hlen; // Hardware address length
    uint8_t plen; // Protocol address length
    uint16_t oper; // Operation code
    mac_addr_t sha; // Sender hardware address
    ipv4_addr_t spa; // Sender protocol address
    mac_addr_t tha; // Target hardware address
    ipv4_addr_t tpa; // Target protocol address
};

int arp_resolve(eth_iface_t * iface, ipv4_addr_t target_ip, mac_addr_t mac) {
    struct arp_pkt request;
    request.hlen = MAC_ADDR_SIZE;
    request.plen = IPv4_ADDR_SIZE;
    request.htype = htons(1); // Ethernet
    request.ptype = htons(0x0800); // IPv4
    request.oper = htons(1); // ARP request
    eth_getaddr(iface, request.sha);
    ipv4_addr_t sender_ip;
    ipv4_str_addr("192.100.100.101", sender_ip);
    memcpy(request.spa, sender_ip, IPv4_ADDR_SIZE);
    memcpy(request.tha, MAC_BCAST_ADDR, MAC_ADDR_SIZE);
    memcpy(request.tpa, target_ip, IPv4_ADDR_SIZE);

    eth_send(iface, MAC_BCAST_ADDR, 0x0806, (unsigned char *)&request, sizeof(struct arp_pkt));

    long int timeout = 2000; // 2 seconds
    timerms_t timer;
    timerms_reset(&timer, timeout);

    unsigned char buffer[ETH_MTU];

    mac_addr_t dummy_source_mac;

    do {
        int len = eth_recv(iface, dummy_source_mac, 0x0806, buffer, sizeof(buffer), timerms_left(&timer));
        if (len <= 0) {
            return -1;
        }
        if (len > 0) {
            if (len < sizeof(struct arp_pkt)) {
                continue;
            }

            struct arp_pkt *arp_reply = (struct arp_pkt *)buffer;
            if (ntohs(arp_reply->oper) == 2 && (memcmp(arp_reply->spa, target_ip, IPv4_ADDR_SIZE) == 0)) {
                memcpy(mac, arp_reply->sha, MAC_ADDR_SIZE);
                return 0;
            }
        }
    } while (timerms_left(&timer) > 0);

    return -1;
}