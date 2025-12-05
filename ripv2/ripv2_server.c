#include "ripv2.h"
#include "ripv2_route_table.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define RIP_TIMEOUT 180

void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip, uint16_t src_port);
void process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip);

int main(int argc, char *argv[]) {
    // 1. Inicialización (Argumentos y UDP Open)
    if (argc != 3) {
        printf("Usage: ./ripv2_server <config> <route_table>\n");
        return -1;
    }
    udp_layer_t *udp_layer = udp_open(argv[1], argv[2]);
    ripv2_route_table_t *rip_table = ripv2_route_table_create();
    
    // (Opcional) Cargar rutas estáticas iniciales en rip_table si se desea

    printf("Servidor RIPv2 escuchando en puerto 520...\n");

    while (1) {
        // 2. Gestión de Temporizadores (Borrar rutas expiradas)
        time_t now = time(NULL);
        // Recorrer rip_table y eliminar entradas donde (now - last_updated) > RIP_TIMEOUT
        // Si hay cambios, imprimir tabla.

        // 3. Recepción UDP (Timeout corto para poder revisar timers frecuentemente)
        uint16_t src_port;
        ipv4_addr_t src_ip;
        unsigned char buffer[1500];
        
        int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, 1500, 1000); // 1s timeout
        
        if (len > 0) {
            ripv2_msg_t *rip_msg = (ripv2_msg_t *)buffer;
            
            // Validar versión y comando
            if (rip_msg->version != 2) continue;

            if (rip_msg->command == RIP_COMMAND_REQUEST) {
                printf("Recibido Request de %d.%d.%d.%d\n", src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
                process_request(udp_layer, rip_table, rip_msg, src_ip, src_port);
            } 
            else if (rip_msg->command == RIP_COMMAND_RESPONSE) {
                printf("Recibido Response de %d.%d.%d.%d\n", src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
                process_response(rip_table, rip_msg, src_ip);
                // Imprimir tabla si hubo cambios
                ripv2_route_table_print(rip_table);
            }
        }
    }
}

// Implementación básica de procesado de Response
void process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip) {
    int entries_count = 25; // Asumimos max o calculamos según len
    for (int i = 0; i < entries_count; i++) {
        ripv2_entry_t *entry = &msg->entries[i];
        if (entry->family == 0) break; // Fin de entradas válidas

        uint32_t new_metric = ntohl(entry->metric) + 1;
        if (new_metric > 16) new_metric = 16;

        // Buscar si ya tenemos esta ruta (Coincidencia exacta de Subnet y Mask)
        ripv2_route_t *route = ripv2_route_table_lookup(table, entry->ip, entry->mask);

        if (route == NULL) {
            // Ruta nueva: Añadir si métrica < 16
            if (new_metric < 16) {
                // Crear ruta, copiar datos, setear next_hop = src_ip, metric = new_metric
                // ripv2_route_table_add(...);
                printf("Nueva ruta aprendida\n");
            }
        } else {
            // Ruta existente: Algoritmo Bellman-Ford
            int from_same_router = (memcmp(route->next_hop, src_ip, 4) == 0);
            
            if (from_same_router) {
                // Si viene del mismo router, actualizamos SIEMPRE (incluso si es peor)
                route->metric = new_metric;
                route->last_updated = time(NULL); // Reset timer
            } else {
                // Si viene de otro router, actualizamos solo si es MEJOR
                if (new_metric < route->metric) {
                    route->metric = new_metric;
                    memcpy(route->next_hop, src_ip, 4);
                    route->last_updated = time(NULL);
                }
            }
        }
    }
}