//
// Created by Lucia Garcia on 17/9/25.
//

#ifndef ETH_BASE_ARP_H
#define ETH_BASE_ARP_H

#include "../eth/eth.h"
#include "../ipv4/ipv4.h"

struct arp_pkt;
int arp_resolve(eth_iface_t * iface, ipv4_addr_t target_ip, mac_addr_t mac);

#endif //ETH_BASE_ARP_H