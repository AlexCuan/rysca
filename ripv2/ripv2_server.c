#include "ripv2.h"
#include "ripv2_route_table.h"
#include "../udp/udp.h"
#include "../utils/rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define RIP_TIMEOUT 180
#define RIP_GARBAGE_SEC 120


// Configuración RIP
#define RIP_UPDATE_INTERVAL 30
#define RIP_JITTER_MAX 5
#define RIP_MCAST_ADDR "224.0.0.9"

// Variables globales para el temporizador
time_t last_update_time = 0;
int current_interval = RIP_UPDATE_INTERVAL;

// PROTOTIPOS
void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, int len, ipv4_addr_t src_ip, uint16_t src_port);
int manage_timers(ripv2_route_table_t *table); // Ahora devuelve int
void send_updates(udp_layer_t *udp, ripv2_route_table_t *table, int is_triggered); // Unificada
void send_initial_request(udp_layer_t *udp);

int main(int argc, char *argv[]) {

    int disable_checksum = 0;
    if (argc > 1 && strcmp(argv[argc-1], "-d") == 0) {
        disable_checksum = 1;
        argc--;
    }

    if (argc < 3 || argc > 4) {
        printf("Uso: ./ripv2_server <config_file> <route_table_file> [rip_routes_file]\n");
        return -1;
    }

    rng_init();

    udp_layer_t *udp_layer = udp_open(argv[1], argv[2], RIP_PORT);
    if (udp_layer == NULL) {
        fprintf(stderr, "ERROR: No se pudo abrir la capa UDP.\n");
        return -1;
    }

    if (disable_checksum) {
        udp_layer->check_checksum = 0;
        printf(">>> AVISO: Verificación de Checksum UDP DESACTIVADA (flag -d) <<<\n");
    }

    ripv2_route_table_t *rip_table = ripv2_route_table_create();
    if (argc == 4) {
        char *rip_routes_file = argv[3];
        ripv2_route_table_read(rip_routes_file, rip_table);
    }

    printf("Servidor RIPv2 arrancado. Escuchando puerto %d...\n", RIP_PORT);

    // 1. Initial Request (Pedir tabla al arrancar)
    send_initial_request(udp_layer);

    // Inicializar timer
    last_update_time = time(NULL);
    current_interval = RIP_UPDATE_INTERVAL + rng_get_rand_in_range(-RIP_JITTER_MAX, RIP_JITTER_MAX);

    while (1) {
        int triggered = 0;

        // 2. Garbage Collection y Timers
        // Si una ruta expira, manage_timers devuelve 1 -> Trigger Update
        if (manage_timers(rip_table)) {
            triggered = 1;
        }

        // 3. Recepción de mensajes (Timeout corto de 500ms)
        uint16_t src_port;
        ipv4_addr_t src_ip;
        unsigned char buffer[1500];

        int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, sizeof(buffer), 500);

        if (len >= 4) {
            ripv2_msg_t *rip_msg = (ripv2_msg_t *)buffer;
            if (rip_msg->version == 2) {
                if (rip_msg->command == RIP_COMMAND_REQUEST) {
                    if (len >= 24) {
                        process_request(udp_layer, rip_table, rip_msg, len, src_ip, src_port);
                    }
                }
                else if (rip_msg->command == RIP_COMMAND_RESPONSE) {
                    // Si process_response devuelve 1 (cambios aprendidos) -> Trigger Update
                    int changes = ripv2_process_response(rip_table, rip_msg, src_ip);
                    if (changes) {
                        printf(">>> TABLA RIPv2 ACTUALIZADA (Nuevas rutas) <<<\n");
                        ripv2_route_table_print(rip_table);
                        triggered = 1;
                    }
                }
            }
        }

        // 4. Gestión de actualizaciones (Triggered o Periódica)
        if (triggered) {
            // TRIGGERED UPDATE: Se envía inmediatamente por cambios
            send_updates(udp_layer, rip_table, 1);
        } else {
            // PERIODIC UPDATE: Se envía solo si expiró el timer
            send_updates(udp_layer, rip_table, 0);
        }
    }
}

/**
 * send_updates:
 * Envía actualizaciones (periódicas o triggered).
 * Fragmenta automáticamente en paquetes de 25 entradas.
 */
void send_updates(udp_layer_t *udp, ripv2_route_table_t *table, int is_triggered) {
    time_t now = time(NULL);

    if (!is_triggered) {
        if ((now - last_update_time) < current_interval) {
            return;
        }
    } else {
        printf("[RIPv2] TRIGGERED UPDATE! Propagando cambios...\n");
    }

    last_update_time = now;
    int jitter = rng_get_rand_in_range(-RIP_JITTER_MAX, RIP_JITTER_MAX);
    current_interval = RIP_UPDATE_INTERVAL + jitter;

    if (!is_triggered) {
        printf("[RIPv2] Periodic Update (Next in %ds)...\n", current_interval);
    }

    ripv2_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.command = RIP_COMMAND_RESPONSE;
    msg.version = RIP_VERSION;

    int entry_count = 0;
    int table_size = ripv2_route_table_size(table);

    ipv4_addr_t dest_ip;
    ipv4_str_addr(RIP_MCAST_ADDR, dest_ip);
    ipv4_addr_t zero_addr = {0,0,0,0};

    for (int i = 0; i < table_size; i++) {
        ripv2_route_t *route = ripv2_route_table_get(table, i);
        if (route == NULL) continue;

        // --- SPLIT HORIZON + POISON REVERSE ---
        uint32_t metric_to_send = route->metric;
        if (memcmp(route->next_hop, zero_addr, 4) != 0) {
            metric_to_send = 16;
        }

        // Si el paquete está lleno, enviar y resetear
        if (entry_count == RIP_MAX_ENTRIES) {
             int len = RIP_HEADER_SIZE + (entry_count * RIP_ENTRY_SIZE);
             udp_send(udp, dest_ip, RIP_PORT, (unsigned char *)&msg, len, 0);

             entry_count = 0;
             memset(msg.entries, 0, sizeof(msg.entries));
        }

        ripv2_entry_t *entry = &msg.entries[entry_count++];
        entry->family = htons(2);
        entry->tag = htons(route->route_tag);
        memcpy(entry->ip, route->subnet, 4);
        memcpy(entry->mask, route->mask, 4);
        memset(entry->next_hop, 0, 4);
        entry->metric = htonl(metric_to_send);
    }

    // Enviar restantes
    if (entry_count > 0) {
        int len = RIP_HEADER_SIZE + (entry_count * RIP_ENTRY_SIZE);
        udp_send(udp, dest_ip, RIP_PORT, (unsigned char *)&msg, len, 0);
    }
}

/**
 * manage_timers:
 * Devuelve 1 si hubo cambios (ruta expirada o borrada), 0 si no.
 */
int manage_timers(ripv2_route_table_t *table) {
    int size = ripv2_route_table_size(table);
    time_t now = time(NULL);
    int changes = 0;

    for (int i = 0; i < size; i++) {
        ripv2_route_t *route = ripv2_route_table_get(table, i);
        if (route == NULL) continue;
        double age = difftime(now, route->last_updated);

        if (!route->is_garbage && age > RIP_TIMEOUT) {
            printf("[TIMER] Ruta %d.%d.%d.%d expirada (>180s). Marcando Garbage.\n",
                   route->subnet[0], route->subnet[1], route->subnet[2], route->subnet[3]);
            route->metric = 16; // Infinito
            route->is_garbage = 1;
            route->last_updated = now; // Reiniciar timer para garbage collection
            changes = 1;
        }
        else if (route->is_garbage && age > RIP_GARBAGE_SEC) {
            printf("[TIMER] Ruta %d.%d.%d.%d eliminada definitivamente (>120s Garbage).\n",
                   route->subnet[0], route->subnet[1], route->subnet[2], route->subnet[3]);
            ripv2_route_t *removed = ripv2_route_table_remove(table, i);
            if (removed) {
                ripv2_route_free(removed);
                changes = 1;
            }
        }
    }

    if (changes) {
        printf("[INFO] Tabla modificada por timers. Disparando Update...\n");
        ripv2_route_table_print(table);
    }
    return changes;
}

void send_initial_request(udp_layer_t *udp) {
    ripv2_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;
    msg.entries[0].family = 0;
    msg.entries[0].metric = htonl(16);

    ipv4_addr_t mcast_addr;
    ipv4_str_addr(RIP_MCAST_ADDR, mcast_addr);

    printf("[RIPv2] Enviando Initial Request...\n");
    udp_send(udp, mcast_addr, RIP_PORT, (unsigned char *)&msg, RIP_HEADER_SIZE + RIP_ENTRY_SIZE, 0);
}

void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, int len, ipv4_addr_t src_ip, uint16_t src_port) {
    int num_entries_req = (len - RIP_HEADER_SIZE) / RIP_ENTRY_SIZE;
    if (num_entries_req <= 0) return;

    // Detectar Whole Table Request
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
        printf("[REQ] Solicitud de tabla COMPLETA recibida de %d.%d.%d.%d\n", src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

        int table_size = ripv2_route_table_size(table);
        for (int i = 0; i < table_size; i++) {
            ripv2_route_t *route = ripv2_route_table_get(table, i);
            if (route == NULL) continue;

            // Si el paquete está lleno (25 entradas), enviamos y reseteamos
            if (response_entries_count == RIP_MAX_ENTRIES) {
                int resp_len = RIP_HEADER_SIZE + (response_entries_count * RIP_ENTRY_SIZE);
                udp_send(udp, src_ip, src_port, (unsigned char *)&response_msg, resp_len, 0);

                // Resetear contador y limpiar entradas
                response_entries_count = 0;
                memset(response_msg.entries, 0, sizeof(response_msg.entries));
            }

            ripv2_entry_t *entry = &response_msg.entries[response_entries_count++];
            entry->family = htons(2);
            entry->tag = htons(route->route_tag);
            memcpy(entry->ip, route->subnet, 4);
            memcpy(entry->mask, route->mask, 4);
            memset(entry->next_hop, 0, 4);
            entry->metric = htonl(route->metric);
        }
    }
    else {
        for (int i = 0; i < num_entries_req; i++) {

            // Si el paquete de RESPUESTA está lleno, lo enviamos
            if (response_entries_count == RIP_MAX_ENTRIES) {
                int resp_len = RIP_HEADER_SIZE + (response_entries_count * RIP_ENTRY_SIZE);
                udp_send(udp, src_ip, src_port, (unsigned char *)&response_msg, resp_len, 0);
                response_entries_count = 0;
                memset(response_msg.entries, 0, sizeof(response_msg.entries));
            }

            ripv2_entry_t *req_entry = &msg->entries[i];
            ripv2_entry_t *resp_entry = &response_msg.entries[response_entries_count++];

            resp_entry->family = htons(2);
            resp_entry->tag = req_entry->tag;
            memcpy(resp_entry->ip, req_entry->ip, 4);
            memcpy(resp_entry->mask, req_entry->mask, 4);
            memset(resp_entry->next_hop, 0, 4);

            ripv2_route_t *route = ripv2_route_table_lookup(table, req_entry->ip, req_entry->mask);
            if (route != NULL) {
                resp_entry->metric = htonl(route->metric);
            } else {
                resp_entry->metric = htonl(16); // No encontrada
            }
        }
    }

    // Enviar las entradas restantes (si quedaron)
    if (response_entries_count > 0) {
        int resp_len = RIP_HEADER_SIZE + (response_entries_count * RIP_ENTRY_SIZE);
        udp_send(udp, src_ip, src_port, (unsigned char *)&response_msg, resp_len, 0);
    }
}
