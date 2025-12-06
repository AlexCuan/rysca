//
// Created by alex on 5/12/25.
//

#ifndef TCP_IP_STACK_RIPV2_H
#define TCP_IP_STACK_RIPV2_H

// TODO: is it necessary stdint?
#include <stdint.h>
#include "../ipv4/ipv4.h"

#define RIP_PORT 520
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2
#define RIP_VERSION 2

typedef struct {
    uint16_t family;      // Address Family Identifier
    uint16_t tag;         // Route Tag
    ipv4_addr_t ip;       // IP Address
    ipv4_addr_t mask;     // Subnet Mask
    ipv4_addr_t next_hop; // Next Hop
    uint32_t metric;      // Metric
} ripv2_entry_t;

typedef struct {
    uint8_t command;
    uint8_t version;
    uint16_t zero;
    ripv2_entry_t entries[25]; // Max 25 entries per packet
} ripv2_msg_t;

#endif //TCP_IP_STACK_RIPV2_H