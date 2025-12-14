
#include "arp.h"
#include "../eth/eth.h"
#include "../ipv4/ipv4.h"
#include <rawnet.h>
#include <timerms.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#define ARP_CACHE_SIZE 16
#define ARP_CACHE_TTL_S 120


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

typedef enum {
    ARP_ENTRY_FREE,
    ARP_ENTRY_RESOLVED
} arp_cache_entry_state_t;

typedef struct {
    arp_cache_entry_state_t state;
    ipv4_addr_t ip_addr;
    mac_addr_t mac_addr;
    time_t timestamp;
} arp_cache_entry_t;

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

static arp_cache_entry_t* arp_cache_find(ipv4_addr_t ip_addr) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].state == ARP_ENTRY_RESOLVED &&
            memcmp(arp_cache[i].ip_addr, ip_addr, IPv4_ADDR_SIZE) == 0) {
            if (time(NULL) - arp_cache[i].timestamp > ARP_CACHE_TTL_S) {
                arp_cache[i].state = ARP_ENTRY_FREE;
                return NULL;
            }
            arp_cache[i].timestamp = time(NULL);
            return &arp_cache[i];
        }
    }
    return NULL;
}

static void arp_cache_add(ipv4_addr_t ip_addr, mac_addr_t mac_addr) {
    int oldest_index = -1;
    // Timestamp del momento
    time_t oldest_time = time(NULL);

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].state == ARP_ENTRY_FREE) {
            oldest_index = i;
            break;
        }
        if (arp_cache[i].timestamp < oldest_time) {
            oldest_time = arp_cache[i].timestamp;
            oldest_index = i;
        }
    }

    if (oldest_index != -1) {
        arp_cache[oldest_index].state = ARP_ENTRY_RESOLVED;
        memcpy(arp_cache[oldest_index].ip_addr, ip_addr, IPv4_ADDR_SIZE);
        memcpy(arp_cache[oldest_index].mac_addr, mac_addr, MAC_ADDR_SIZE);
        arp_cache[oldest_index].timestamp = time(NULL);
    }
}


/* int arp_resolve(eth_iface_t * iface, ipv4_addr_t src_ip, ipv4_addr_t target_ip, mac_addr_t mac)

 * DESCRIPCIÓN:
 *   Esta función resuelve una dirección IPv4 a una dirección MAC utilizando ARP.
 *
 * PARÁMETROS:
 *   'iface': Interfaz Ethernet a través de la cual enviar la solicitud ARP.
 *   'src_ip': Direccion IPv4 origen
 *   'target_ip': Dirección IPv4 a resolver.
 *   'mac': Buffer donde se almacenará la dirección MAC resuelta.
 *
 * VALOR DEVUELTO:
 *   0 si la dirección MAC se resolvió correctamente, -1 en caso de error, -2
 *   tiempo de espera agotado.
 */

int arp_resolve(eth_iface_t * iface, ipv4_addr_t src_ip, ipv4_addr_t target_ip, mac_addr_t target_mac, mac_addr_t mac) {
    arp_cache_entry_t* entry = arp_cache_find(target_ip);
    if (entry != NULL) {
        memcpy(mac, entry->mac_addr, MAC_ADDR_SIZE);
        return 0;
    }

    struct arp_pkt request;
    request.hlen = MAC_ADDR_SIZE;
    request.plen = IPv4_ADDR_SIZE;
    request.htype = htons(1); // Ethernet
    request.ptype = htons(0x0800); // IPv4
    request.oper = htons(1); // ARP request
    eth_getaddr(iface, request.sha);
    memcpy(request.spa, src_ip, IPv4_ADDR_SIZE);
    memcpy(request.tpa, target_ip, IPv4_ADDR_SIZE);

    mac_addr_t dest_mac;
    if (target_mac != NULL) {
        memcpy(dest_mac, target_mac, MAC_ADDR_SIZE);
        memcpy(request.tha, target_mac, MAC_ADDR_SIZE);
    } else {
        memcpy(dest_mac, MAC_BCAST_ADDR, MAC_ADDR_SIZE);
        memset(request.tha, 0, MAC_ADDR_SIZE);
    }

    unsigned char buffer[ETH_MTU];
    mac_addr_t dummy_source_mac;
    timerms_t timer;
    long int timeouts[] = {2000, 3000};
    int num_retries = 2;
    int i;

    for (i = 0; i < num_retries; i++) {
        eth_send(iface, dest_mac, 0x0806, (unsigned char *)&request, sizeof(struct arp_pkt));
        timerms_reset(&timer, timeouts[i]);

        do {
            long int time_left = timerms_left(&timer);
            if (time_left <= 0) {
                break;
            }

            int len = eth_recv(iface, dummy_source_mac, 0x0806, buffer, sizeof(buffer), time_left);

            if (len < 0) {
                return -1;
            }

            if (len == 0) {
                continue;
            }

            if (len < sizeof(struct arp_pkt)) {
                continue;
            }

            struct arp_pkt *arp_reply = (struct arp_pkt *)buffer;
            if (ntohs(arp_reply->oper) == 2 && (memcmp(arp_reply->spa, target_ip, IPv4_ADDR_SIZE) == 0)) {
                memcpy(mac, arp_reply->sha, MAC_ADDR_SIZE);
                arp_cache_add(target_ip, mac);
                return 0;
            }
        } while (1);
    }

    return -2;
}
