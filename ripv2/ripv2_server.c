#include "ripv2.h"
#include "ripv2_route_table.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define RIP_TIMEOUT 180      // 180s para declarar ruta inválida (metric 16)
#define RIP_GARBAGE_SEC 120  // 120s extra para borrarla definitivamente

void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip, uint16_t src_port);
int process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip);
void manage_timers(ripv2_route_table_t *table);

int main(int argc, char *argv[]) {
    // Validación de argumentos
    if (argc != 3) {
        printf("Uso: ./ripv2_server <config_file> <route_table_file>\n");
        return -1;
    }

    // Inicializar capas con comprobación de errores
    udp_layer_t *udp_layer = udp_open(argv[1], argv[2]);
    if (udp_layer == NULL) {
        fprintf(stderr, "ERROR: No se pudo abrir la capa UDP. Revisa ficheros de configuración.\n");
        return -1;
    }

    ripv2_route_table_t *rip_table = ripv2_route_table_create();
    // (Opcional) Aquí podrías cargar rutas iniciales si el enunciado lo pidiera
    // ripv2_route_table_read(argv[2], rip_table);

    printf("Servidor RIPv2 arrancado. Escuchando puerto 520...\n");

    while (1) {
        // 2. Gestión de Temporizadores (Requisito: Borrar entradas antiguas)
        manage_timers(rip_table);

        // 3. Recepción UDP con timeout corto (1s) para poder atender timers
        uint16_t src_port;
        ipv4_addr_t src_ip;
        unsigned char buffer[1500];
        memset(buffer, 0, sizeof(buffer));
        // Timeout de 1000ms para no bloquear eternamente y poder ejecutar manage_timers
        int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, sizeof(buffer), 1000);

    if (len >= 4) { // Mínimo tamaño de cabecera RIP (4 bytes)

    ripv2_msg_t *rip_msg = (ripv2_msg_t *)buffer;

    // Validar versión
    if (rip_msg->version != 2) continue;

    if (rip_msg->command == RIP_COMMAND_REQUEST) {
        // Validar que hay al menos 1 entrada (4 header + 20 entry = 24 bytes)
        if (len >= 24) {
            process_request(udp_layer, rip_table, rip_msg, src_ip, src_port);
        }
    }
    else if (rip_msg->command == RIP_COMMAND_RESPONSE) {
        printf("[DEBUG] Recibido RIP Response de %d.%d.%d.%d\n",
               src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

        // Llamar a la función que procesa la respuesta
        int changes = process_response(rip_table, rip_msg, src_ip);

        // Requisito del enunciado: "imprimir... el estado final de la misma una vez aplicados todos los cambios"
        if (changes) {
            printf(">>> TABLA RIPv2 ACTUALIZADA <<<\n");
            ripv2_route_table_print(rip_table);
        }
    }
    }}}

/*
 * Procesa un REQUEST.
 * Si es una petición de toda la tabla (Family 0, Metric 16), envía todo.
 */
void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip, uint16_t src_port) {

    // Verificar si es "Whole Table Request" (RFC 2453)
    int request_all = 0;
    if (ntohs(msg->entries[0].family) == 0 && ntohl(msg->entries[0].metric) == 16) {
        request_all = 1;
    }

    if (!request_all) return; // Simplificación: solo atendemos peticiones completas

    printf("Recibido Request de toda la tabla desde %d.%d.%d.%d\n",
           src_ip[0], src_ip[1], src_ip[2], src_ip[3]);

    ripv2_msg_t response_msg;
    memset(&response_msg, 0, sizeof(response_msg));
    response_msg.command = RIP_COMMAND_RESPONSE;
    response_msg.version = 2;

    int response_entries = 0;
    int table_size = ripv2_route_table_size(table);

    // Iterar tabla para llenar el paquete
    for (int i = 0; i < table_size; i++) {
        ripv2_route_t *route = ripv2_route_table_get(table, i);

        if (route != NULL) {
            // SEGURIDAD: Evitar desbordamiento si hay > 25 rutas
            if (response_entries >= 25) break;

            ripv2_entry_t *entry = &response_msg.entries[response_entries++];

            entry->family = htons(2); // AF_INET
            entry->tag = htons(route->route_tag);
            memcpy(entry->ip, route->subnet, 4);
            memcpy(entry->mask, route->mask, 4);
            // En Response, next_hop debe ser 0.0.0.0 si somos nosotros el GW
            memset(entry->next_hop, 0, 4);
            entry->metric = htonl(route->metric);
        }
    }

    // Calcular longitud exacta del paquete UDP
    int response_len = 4 + (response_entries * 20); // Header (4) + Entradas
    udp_send(udp, src_ip, src_port, (unsigned char *)&response_msg, response_len);
}

/*
 * Procesa un RESPONSE.
 * Implementa el algoritmo Bellman-Ford y actualiza timers.
 */
int process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip) {
    int changes = 0;

    // Iterar sobre las entradas del mensaje (máximo 25)
    for (int i = 0; i < 25; i++) {
        ripv2_entry_t *entry = &msg->entries[i];

        // Si la familia no es IP (2), asumimos fin de lista o entrada inválida
        if (ntohs(entry->family) != 2) continue;

        uint32_t received_metric = ntohl(entry->metric);
        uint32_t new_metric = received_metric + 1; // Coste del enlace = 1
        if (new_metric > 16) new_metric = 16;      // Infinito

        // Determinar Next Hop real (RFC 2453 Sec 4.4)
        ipv4_addr_t real_next_hop;
        ipv4_addr_t zero_addr = {0,0,0,0};

        if (memcmp(entry->next_hop, zero_addr, 4) == 0) {
            memcpy(real_next_hop, src_ip, 4); // El emisor es el gateway
        } else {
            memcpy(real_next_hop, entry->next_hop, 4); // El emisor sugiere otro gateway
        }

        // Buscar ruta existente
        ripv2_route_t *route = ripv2_route_table_lookup(table, entry->ip, entry->mask);

        if (route == NULL) {
            // --- RUTA NUEVA ---
            if (new_metric < 16) {
                // Crear y añadir la ruta usando las funciones del TAD
                ripv2_route_t *new_route = ripv2_route_create(entry->ip, entry->mask, real_next_hop, new_metric);
                if (ripv2_route_table_add(table, new_route) != -1) {
                    changes = 1;
                    printf("Nueva ruta aprendida: "); ripv2_route_print(new_route);
                } else {
                    free(new_route);
                }
            }
        } else {
            // --- RUTA EXISTENTE ---
            int from_same_router = (memcmp(route->next_hop, real_next_hop, 4) == 0);

            if (from_same_router) {
                // A) Viene del mismo router: Actualizar SIEMPRE (incluso si empeora)
                // y resetear timer
                if (route->metric != new_metric) {
                    route->metric = new_metric;
                    changes = 1;
                }
                route->last_updated = time(NULL);
                route->is_garbage = 0; // Revivir si estaba en garbage
            } else {
                // B) Viene de otro router: Actualizar SOLO si es MEJOR
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

/*
 * Gestión de temporizadores: Timeout y Garbage Collection
 */
void manage_timers(ripv2_route_table_t *table) {
    int size = ripv2_route_table_size(table);
    time_t now = time(NULL);
    int changes = 0;

    for (int i = 0; i < size; i++) {
        ripv2_route_t *route = ripv2_route_table_get(table, i);
        if (route == NULL) continue;

        double age = difftime(now, route->last_updated);

        // Fase 1: Timeout (180s) -> Marcar como inalcanzable
        if (!route->is_garbage && age > RIP_TIMEOUT) {
            printf("[TIMER] Ruta expirada (Timeout > 180s). Métrica puesta a 16.\n");
            route->metric = 16;
            route->is_garbage = 1;
            route->last_updated = now; // Reiniciar cuenta para garbage
            changes = 1;
        }

        // Fase 2: Garbage Collection (120s extra) -> Borrar de tabla
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
