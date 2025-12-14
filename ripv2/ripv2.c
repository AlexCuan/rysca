#include "ripv2.h"
#include "ripv2_route_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * Procesa un mensaje RIP Response y actualiza la tabla de rutas.
 * Devuelve 1 si hubo cambios en la tabla, 0 si no.
 */
int ripv2_process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip) {
    int changes = 0;

    for (int i = 0; i < RIP_MAX_ENTRIES; i++) {
        ripv2_entry_t *entry = &msg->entries[i];

        if (ntohs(entry->family) != 2) continue;

        uint32_t received_metric = ntohl(entry->metric);
        uint32_t new_metric = received_metric + 1;
        if (new_metric > 16) new_metric = 16;

        ipv4_addr_t real_next_hop;
        ipv4_addr_t zero_addr = {0,0,0,0};

        if (memcmp(entry->next_hop, zero_addr, 4) == 0) {
            memcpy(real_next_hop, src_ip, 4);
        } else {
            memcpy(real_next_hop, entry->next_hop, 4);
        }

        ripv2_route_t *route = ripv2_route_table_lookup(table, entry->ip, entry->mask);

        if (route == NULL) {
            if (new_metric < 16) {
                ripv2_route_t *new_route = ripv2_route_create(entry->ip, entry->mask, real_next_hop, new_metric);
                if (ripv2_route_table_add(table, new_route) != -1) {
                    changes = 1;
                } else {
                    free(new_route);
                }
            }
        } else {
            int from_same_router = (memcmp(route->next_hop, real_next_hop, 4) == 0);

            if (from_same_router) {
                if (route->metric != new_metric) {
                    route->metric = new_metric;
                    changes = 1;
                }
                route->last_updated = time(NULL);

                if (new_metric < 16) {
                    route->is_garbage = 0;
                } else {
                    if (!route->is_garbage) {
                        route->is_garbage = 1;
                        changes = 1;
                    }
                }
            } else {
                if (new_metric < route->metric) {
                    route->metric = new_metric;
                    memcpy(route->next_hop, real_next_hop, 4);
                    route->last_updated = time(NULL);
                    route->is_garbage = 0;
                    changes = 1;
                }
            }
        }
    }
    return changes;
}