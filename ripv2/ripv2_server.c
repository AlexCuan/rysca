#include "ripv2.h"
#include "ripv2_route_table.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define RIP_TIMEOUT 180
#define RIP_GARBAGE_SEC 120

// PROTOTIPOS ACTUALIZADOS
// Se añade el parámetro 'len' para saber cuántas entradas llegan en el Request
void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, int len, ipv4_addr_t src_ip, uint16_t src_port);
int process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip);
void manage_timers(ripv2_route_table_t *table);
void send_initial_request(udp_layer_t *udp_layer); // Nuevo prototipo

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: ./ripv2_server <config_file> <route_table_file>\n");
        return -1;
    }

    udp_layer_t *udp_layer = udp_open(argv[1], argv[2], RIP_PORT);
    if (udp_layer == NULL) {
        fprintf(stderr, "ERROR: No se pudo abrir la capa UDP. Revisa ficheros de configuración.\n");
        return -1;
    }

    ripv2_route_table_t *rip_table = ripv2_route_table_create();
    printf("Servidor RIPv2 arrancado. Escuchando puerto %d...\n", RIP_PORT);
    send_initial_request(udp_layer);

    while (1) {
        printf("\n--- Estado actual de la tabla RIPv2 ---\n");
        ripv2_route_table_print(rip_table);
        printf("-----------------------------------------------------------\n");

        manage_timers(rip_table);

        uint16_t src_port;
        ipv4_addr_t src_ip;
        unsigned char buffer[1500];
        memset(buffer, 0, sizeof(buffer));

        int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, sizeof(buffer), 1000);

        if (len >= 4) {
            ripv2_msg_t *rip_msg = (ripv2_msg_t *)buffer;
            if (rip_msg->version != 2) continue;

            if (rip_msg->command == RIP_COMMAND_REQUEST) {
                if (len >= 24) {
                    // ACTUALIZADO: Pasamos 'len' a process_request
                    process_request(udp_layer, rip_table, rip_msg, len, src_ip, src_port);
                }
            }
            else if (rip_msg->command == RIP_COMMAND_RESPONSE) {
                printf("[DEBUG] Recibido RIP Response de %d.%d.%d.%d\n",
                       src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

                int changes = process_response(rip_table, rip_msg, src_ip);
                if (changes) {
                    printf(">>> TABLA RIPv2 ACTUALIZADA TRAS RESPONSE <<<\n");
                    ripv2_route_table_print(rip_table);
                }
            }
        }
    }
}

/**
 * process_request: Maneja tanto Whole Table Requests como peticiones específicas.
 */
void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, int len, ipv4_addr_t src_ip, uint16_t src_port) {

    // Calcular número de entradas en el mensaje recibido
    // len total = header (4) + N * entry (20)
    int num_entries_req = (len - RIP_HEADER_SIZE) / RIP_ENTRY_SIZE;
    if (num_entries_req <= 0) return;

    // Verificar si es "Whole Table Request" (Familia 0, Métrica 16)
    int request_all = 0;
    if (num_entries_req == 1 && ntohs(msg->entries[0].family) == 0 && ntohl(msg->entries[0].metric) == 16) {
        request_all = 1;
    }

    ripv2_msg_t response_msg;
    memset(&response_msg, 0, sizeof(response_msg));
    response_msg.command = RIP_COMMAND_RESPONSE;
    response_msg.version = 2;
    int response_entries_count = 0;

    if (request_all) {
        // --- CASO 1: Solicitud de Tabla Completa ---
        printf("Recibido Request de TODA la tabla desde %d.%d.%d.%d\n",
               src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

        int table_size = ripv2_route_table_size(table);
        for (int i = 0; i < table_size; i++) {
            ripv2_route_t *route = ripv2_route_table_get(table, i);
            if (route != NULL) {
                if (response_entries_count >= 25) break;

                ripv2_entry_t *entry = &response_msg.entries[response_entries_count++];
                entry->family = htons(2);
                entry->tag = htons(route->route_tag);
                memcpy(entry->ip, route->subnet, 4);
                memcpy(entry->mask, route->mask, 4);
                memset(entry->next_hop, 0, 4); // NextHop 0.0.0.0 (nosotros)
                entry->metric = htonl(route->metric);
            }
        }

    } else {
        // --- CASO 2: Solicitud de Rutas Específicas (Partial Update) ---
        printf("Recibido Request PARCIAL (%d entradas) desde %d.%d.%d.%d\n",
               num_entries_req, src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

        // Iterar sobre las entradas que nos piden (máximo 25)
        for (int i = 0; i < num_entries_req && i < 25; i++) {
            ripv2_entry_t *req_entry = &msg->entries[i];
            ripv2_entry_t *resp_entry = &response_msg.entries[response_entries_count++];

            // Copiamos la info básica de la petición a la respuesta
            resp_entry->family = htons(2); // Siempre AF_INET
            resp_entry->tag = req_entry->tag;
            memcpy(resp_entry->ip, req_entry->ip, 4);
            memcpy(resp_entry->mask, req_entry->mask, 4);
            memset(resp_entry->next_hop, 0, 4);

            // Buscamos la ruta en nuestra tabla
            // NOTA: Se requiere coincidencia exacta de Subnet y Máscara
            ripv2_route_t *route = ripv2_route_table_lookup(table, req_entry->ip, req_entry->mask);

            if (route != NULL) {
                // Ruta encontrada: devolvemos nuestra métrica
                resp_entry->metric = htonl(route->metric);
            } else {
                // Ruta NO encontrada: devolvemos infinito (16)
                resp_entry->metric = htonl(16);
            }
        }
    }

    // Enviar respuesta si generamos alguna entrada
    if (response_entries_count > 0) {
        int response_len = RIP_HEADER_SIZE + (response_entries_count * RIP_ENTRY_SIZE);
        udp_send(udp, src_ip, src_port, (unsigned char *)&response_msg, response_len, 0);
        printf("Enviado Response con %d entradas.\n", response_entries_count);
    }
}

// ... (El resto del archivo: process_response y manage_timers se mantienen igual) ...
int process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip) {
    int changes = 0;
    for (int i = 0; i < 25; i++) {
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
                    printf("Nueva ruta aprendida: "); ripv2_route_print(new_route);
                } else {
                    free(new_route);
                }
            }
        } else {
            int from_same_router = (memcmp(route->next_hop, real_next_hop, 4) == 0);
            if (from_same_router) {
                uint32_t old_metric = route->metric;
                if (route->metric != new_metric) {
                    route->metric = new_metric;
                    changes = 1;
                }
                if (new_metric < 16) {
                    route->last_updated = time(NULL);
                    route->is_garbage = 0;
                } else {
                    if (old_metric < 16) {
                        printf("[RIP] Ruta %d.%d.%d.%d ha muerto (Métrica 16).\n",
                               route->subnet[0], route->subnet[1], route->subnet[2], route->subnet[3]);
                        route->last_updated = time(NULL);
                        route->is_garbage = 1;
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

void manage_timers(ripv2_route_table_t *table) {
    int size = ripv2_route_table_size(table);
    time_t now = time(NULL);
    int changes = 0;

    for (int i = 0; i < size; i++) {
        ripv2_route_t *route = ripv2_route_table_get(table, i);
        if (route == NULL) continue;
        double age = difftime(now, route->last_updated);

        if (!route->is_garbage && age > RIP_TIMEOUT) {
            printf("[TIMER] Ruta expirada (Timeout > 180s). Métrica puesta a 16.\n");
            route->metric = 16;
            route->is_garbage = 1;
            route->last_updated = now;
            changes = 1;
        }
        else if (route->is_garbage && age > RIP_GARBAGE_SEC) {
            printf("[TIMER] Ruta eliminada definitivamente (Garbage Collection).\n");
            ripv2_route_t *removed = ripv2_route_table_remove(table, i);
            if (removed) {
                ripv2_route_free(removed);
                changes = 1;
            }
        }
    }

    if (changes) {
        printf("[INFO] Tabla actualizada por expiración de timers:\n");
        ripv2_route_table_print(table);
    }
}

/**
 * NUEVA FUNCIÓN: Envía un RIP Request solicitando la tabla completa.
 * Destino: 224.0.0.9 (Multicast)
 * Contenido: Una entrada con Family=0 y Metric=16 (infinito)
 */
void send_initial_request(udp_layer_t *udp_layer) {
    ripv2_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;

    // Entrada especial para solicitar tabla completa (RFC 2453, sec 3.9.1)
    msg.entries[0].family = 0; // Family 0
    msg.entries[0].metric = htonl(16); // Metric infinity

    int payload_len = RIP_HEADER_SIZE + RIP_ENTRY_SIZE;

    ipv4_addr_t mcast_addr;
    if (ipv4_str_addr(RIP_MCAST_ADDR, mcast_addr) == 0) {
        printf("[RIPv2] Enviando Petición Inicial de Tabla a %s...\n", RIP_MCAST_ADDR);
        // Enviamos al puerto RIP (520)
        udp_send(udp_layer, mcast_addr, RIP_PORT, (unsigned char *)&msg, payload_len, 0);
    } else {
        fprintf(stderr, "[RIPv2] Error parseando dirección multicast.\n");
    }
}