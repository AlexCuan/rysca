#include "ripv2.h"
#include "ripv2_route_table.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define RIP_TIMEOUT 180      // 180s para declarar ruta inválida (metric 16)
#define RIP_GARBAGE_SEC 240

void process_request(udp_layer_t *udp, ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip, uint16_t src_port);
int process_response(ripv2_route_table_t *table, ripv2_msg_t *msg, ipv4_addr_t src_ip);
void manage_timers(ripv2_route_table_t *table);

/**
 * @brief Punto de entrada principal del servidor RIPv2.
 *
 * Esta función inicializa el servidor y ejecuta el bucle principal de procesamiento.
 *
 * Pasos de inicialización:
 * 1. Valida los argumentos de la línea de comandos (fichero de configuración y tabla de rutas).
 * 2. Inicializa la capa UDP (`udp_open`) utilizando los ficheros proporcionados.
 * 3. Crea e inicializa la tabla de rutas RIPv2 vacía (`ripv2_route_table_create`).
 *
 * Bucle principal (infinito):
 * El servidor entra en un bucle donde realiza continuamente las siguientes tareas:
 *
 * 1. Gestión de Temporizadores (`manage_timers`):
 *    - Verifica si hay rutas que han expirado (Timeout) o deben ser eliminadas (Garbage Collection).
 *
 * 2. Recepción de Mensajes UDP:
 *    - Espera mensajes en el puerto 520 (puerto estándar RIP).
 *    - Utiliza un timeout de 1000ms (1 segundo) en `udp_rcv`. Esto es fundamental para
 *      evitar que el servidor se bloquee indefinidamente esperando paquetes y pueda
 *      seguir atendiendo los temporizadores periódicamente.
 *
 * 3. Procesamiento de Mensajes:
 *    - Si se recibe un paquete válido (longitud mínima de cabecera):
 *      a. Valida que la versión del protocolo sea RIPv2 (`version == 2`).
 *      b. Identifica el tipo de comando:
 *         - RIP_COMMAND_REQUEST (1): Solicitud de información de enrutamiento.
 *           Se llama a `process_request` para responder.
 *         - RIP_COMMAND_RESPONSE (2): Actualización de rutas desde otro router.
 *           Se llama a `process_response` para actualizar la tabla local.
 *           Si la tabla cambia, se imprime su nuevo estado.
 *
 * @param argc Número de argumentos.
 * @param argv Argumentos: [1] fichero config UDP, [2] fichero tabla rutas (o config asociada).
 * @return 0 si finaliza correctamente, -1 en caso de error.
 */
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
        // Imprimir la tabla de rutas en cada iteración
        printf("\n--- Estado actual de la tabla RIPv2 ---\n");
        ripv2_route_table_print(rip_table);
        printf("-----------------------------------------------------------\n");

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
            printf(">>> TABLA RIPv2 ACTUALIZADA TRAS RESPONSE <<<\n");
            ripv2_route_table_print(rip_table);
        }
    }
    }}}

/**
 * @brief Procesa un mensaje RIPv2 de tipo REQUEST.
 *
 * Esta función maneja las solicitudes de información de enrutamiento provenientes
 * de otros routers. Según el RFC 2453, existen dos tipos de solicitudes:
 * 1. Solicitud de tabla completa: Se identifica por tener una única entrada con
 *    familia 0 y métrica 16 (infinito).
 * 2. Solicitud de rutas específicas: Lista de entradas para las que se pide información.
 *
 * Implementación actual:
 * - Solo soporta "Whole Table Requests" (Solicitud de tabla completa). Si recibe
 *   una solicitud parcial, la ignora (simplificación).
 *
 * Funcionamiento:
 * 1. Verifica si el mensaje es una solicitud de tabla completa.
 * 2. Si lo es, construye un mensaje de respuesta (RESPONSE) que contiene la
 *    información de todas las rutas conocidas en la tabla local.
 * 3. Itera sobre la tabla de rutas y añade entradas al mensaje de respuesta.
 *    - Se limita a 25 entradas por paquete (límite del protocolo RIP).
 *    - Configura la familia a AF_INET (2).
 *    - Copia la subred, máscara y métrica actual.
 *    - Establece el `next_hop` a 0.0.0.0, indicando que el receptor debe enviar
 *      los paquetes a la dirección IP de origen de este mensaje (nosotros).
 * 4. Envía el mensaje de respuesta vía UDP a la dirección y puerto del solicitante.
 *
 * @param udp Puntero a la capa UDP para enviar la respuesta.
 * @param table Puntero a la tabla de rutas local.
 * @param msg Puntero al mensaje RIPv2 de solicitud recibido.
 * @param src_ip Dirección IP del router que envió la solicitud.
 * @param src_port Puerto UDP del router que envió la solicitud.
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
    udp_send(udp, 520, src_ip, src_port, (unsigned char *)&response_msg, response_len);
}

/**
 * @brief Procesa un mensaje RIPv2 de tipo RESPONSE para actualizar la tabla de rutas.
 *
 * Esta función es el núcleo del algoritmo de vector-distancia (Bellman-Ford) en RIP.
 * Itera sobre cada entrada de ruta (RTE) en el mensaje de respuesta recibido y
 * decide si la tabla de enrutamiento local debe ser actualizada.
 *
 * El proceso para cada entrada del mensaje es el siguiente:
 * 1.  Calcula la nueva métrica: `new_metric = metric_recibida + 1`. El "+1" representa
 *     el coste de saltar al router que envió el mensaje. La métrica se limita a 16
 *     (infinito) si el cálculo la excede.
 *
 * 2.  Determina el "siguiente salto" (`next_hop`) real. Según el RFC 2453, si el campo
 *     `next_hop` en la entrada es 0.0.0.0, significa que el verdadero `next_hop` es
 *     la IP de origen del paquete. De lo contrario, se usa el valor especificado.
 *
 * 3.  Busca en la tabla de rutas local si ya existe una ruta hacia la misma subred.
 *
 * 4.  Toma de decisiones:
 *     a. Si la ruta NO existe en la tabla local (`route == NULL`):
 *        - Si la `new_metric` es menor que 16 (no es inalcanzable), se crea una
 *          nueva entrada en la tabla de rutas local con la información recibida
 *          (subred, máscara, `next_hop` real y `new_metric`).
 *
 *     b. Si la ruta SÍ existe:
 *        - Se comprueba si el anuncio proviene del MISMO router que ya se usa
 *          como `next_hop` para esa ruta.
 *          - Si es el mismo router: Se actualiza la entrada local SIEMPRE,
 *            incluso si la nueva métrica es peor. Esto es crucial para propagar
 *            rápidamente información sobre rutas que empeoran o se vuelven
 *            inalcanzables (métrica 16). El temporizador de expiración de la
 *            ruta se resetea.
 *          - Si es un router DIFERENTE: Se actualiza la entrada local SOLO SI la
 *            `new_metric` es estrictamente MEJOR (menor) que la métrica actual.
 *            Si se actualiza, se cambia tanto la métrica como el `next_hop` y se
 *            resetea el temporizador.
 *
 * @param table Puntero a la tabla de rutas RIPv2 que se va a procesar.
 * @param msg Puntero al mensaje RIPv2 de respuesta recibido.
 * @param src_ip Dirección IP del router que envió el mensaje de respuesta.
 * @return Devuelve 1 si se realizó algún cambio en la tabla de rutas, 0 en caso contrario.
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
                // Guardamos el estado anterior para saber si hubo transición
                uint32_t old_metric = route->metric;

                // Actualizar métrica (siempre aceptamos la del next_hop)
                if (route->metric != new_metric) {
                    route->metric = new_metric;
                    changes = 1;
                }

                if (new_metric < 16) {
                    // CASO A: Ruta sana. Reiniciamos timer siempre (es un "estoy vivo").
                    route->last_updated = time(NULL);
                    route->is_garbage = 0;
                }
                else {
                    // CASO B: Ruta infinita (16).

                    // Solo reiniciamos el timer si la ruta NO era infinita antes.
                    // Es decir, es la PRIMERA vez que nos dicen que murió.
                    if (old_metric < 16) {
                        printf("[RIP] Ruta %d.%d.%d.%d ha muerto (Métrica 16). Iniciando cuenta de %ds.\n",
                               route->subnet[0], route->subnet[1], route->subnet[2], route->subnet[3], RIP_TIMEOUT);
                        route->last_updated = time(NULL); // Empezamos a contar 0 -> 180
                        route->is_garbage = 0;
                    }
                    // Si old_metric ya era 16 y new_metric es 16, NO hacemos nada con el timer.
                    // Dejamos que el tiempo siga corriendo para que manage_timers llegue a 180s.
                }
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

/**
 * @brief Revisa la tabla de rutas para gestionar la expiración de las entradas.
 *
 * Esta función implementa los dos temporizadores clave del protocolo RIP:
 * el "Timeout" y el "Garbage Collection timer". Se debe llamar periódicamente
 * para mantener la tabla de rutas actualizada y eliminar rutas obsoletas.
 *
 * El proceso es el siguiente:
 * 1. Itera sobre cada una de las rutas en la tabla.
 * 2. Calcula el tiempo transcurrido ('age') desde la última vez que la ruta
 *    fue actualizada (`last_updated`).
 *
 * 3. Fase de Timeout (Invalidación):
 *    - Si una ruta no ha sido actualizada en `RIP_TIMEOUT` (180 segundos) y
 *      aún no está en proceso de borrado (`is_garbage` es falso):
 *      a. Se considera que la ruta ha expirado.
 *      b. Su métrica se establece en 16 (infinito), marcándola como inalcanzable.
 *      c. Se activa el flag `is_garbage` para indicar que ha entrado en la
 *         fase de borrado.
 *      d. Se actualiza `last_updated` al tiempo actual. Esto es crucial, ya que
 *         el contador para la siguiente fase (Garbage Collection) empieza ahora.
 *
 * 4. Fase de Garbage Collection (Borrado definitivo):
 *    - Si una ruta ya está marcada como `is_garbage` y han pasado
 *      `RIP_GARBAGE_SEC` (120 segundos) desde que se marcó:
 *      a. Se considera que el tiempo de espera para recibir posibles actualizaciones
 *         contradictorias ha terminado.
 *      b. La ruta se elimina permanentemente de la tabla de enrutamiento.
 *      c. Se libera la memoria asociada a la ruta eliminada.
 *
 * 5. Si se ha producido algún cambio (rutas invalidadas o eliminadas), se
 *    imprime la tabla de rutas actualizada para reflejar el estado actual.
 *
 * @param table Puntero a la tabla de rutas RIPv2 que se va a gestionar.
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
