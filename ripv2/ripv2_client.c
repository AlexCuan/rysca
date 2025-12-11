#include "ripv2.h"
#include "../udp/udp.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * @brief Punto de entrada principal del cliente RIPv2.
 *
 * Este programa actúa como un cliente de diagnóstico que envía una única solicitud
 * RIPv2 a un servidor específico para obtener su tabla de enrutamiento completa y
 * luego la imprime en la consola.
 *
 * Funcionamiento:
 * 1.  **Análisis de Argumentos**:
 *     - Valida que se proporcionen los tres argumentos necesarios: el fichero de
 *       configuración de la capa inferior (UDP/IP), un fichero de rutas (aunque
 *       no se use activamente para enviar, es requerido por la capa UDP), y la
 *       dirección IP del servidor RIP al que se le hará la consulta.
 *
 * 2.  **Inicialización de la Capa UDP**:
 *     - Abre la capa UDP con los ficheros de configuración especificados.
 *
 * 3.  **Construcción del Mensaje de Solicitud (Request)**:
 *     - Crea un mensaje RIPv2 de tipo `REQUEST` (comando 1).
 *     - Para solicitar la tabla de rutas completa, se añade una única entrada
 *       especial, como define el RFC 2453:
 *         - `family` se establece en 0.
 *         - `metric` se establece en 16 (infinito).
 *
 * 4.  **Envío de la Solicitud**:
 *     - Envía el mensaje construido a la dirección IP del servidor especificada
 *       y al puerto estándar de RIP (520).
 *
 * 5.  **Recepción de la Respuesta**:
 *     - Espera recibir una respuesta del servidor con un timeout de 5 segundos.
 *     - Incluye mensajes de depuración para saber cuántos bytes se recibieron.
 *
 * 6.  **Procesamiento de la Respuesta**:
 *     - Si se recibe una respuesta y es del tipo `RESPONSE` (comando 2):
 *       a. Calcula el número de entradas de ruta basándose en la longitud del paquete.
 *       b. Itera sobre cada entrada e imprime sus detalles:
 *          - Subred y máscara.
 *          - Métrica (coste para llegar a esa subred).
 *          - Siguiente salto (Next Hop): Interpreta el campo `next_hop`. Si es
 *            "0.0.0.0", significa que el `next_hop` real es el propio servidor que
 *            envió la respuesta. Si tiene otra IP, se muestra esa.
 *     - Si no se recibe respuesta o el paquete no es una respuesta RIP válida,
 *       muestra un mensaje de error informativo.
 *
 * 7.  **Cierre**:
 *     - Cierra la capa UDP y finaliza el programa.
 *
 * @param argc Número de argumentos.
 * @param argv Argumentos: [1] config_file, [2] routes_file, [3] server_ip.
 * @return 0 si la operación fue exitosa, -1 en caso de error.
 */
int main(int argc, char *argv[]) {
    // 1. Argument parsing
    if (argc != 4) {
        printf("Ussage: ./ripv2_client <config_file> <routes_file> <server_ip>\n");
        return -1;
    }
    char *config = argv[1];
    char *routes = argv[2];
    char *server_ip_str = argv[3]; // La IP de R1 (a quien le preguntamos)

    // 2. Open UDP Layer
    udp_layer_t *udp_layer = udp_open(config, routes);

    if (!udp_layer) {
        perror("Failed to open UDP layer");
        return -1;
    }
    ipv4_addr_t dest_ip;

    if(ipv4_str_addr(server_ip_str, dest_ip) != 0)
    {
        fprintf(stderr, "ERROR: Invalid server IP address '%s'\n", server_ip_str);
        return -1;
    };

    ripv2_msg_t msg = {0};
    msg.command = RIP_COMMAND_REQUEST;
    msg.version = RIP_VERSION;

    // Special request for whole table: Family 0, Metric 16
    msg.entries[0].family = 0;
    msg.entries[0].metric = htonl(16); // Network Byte Order

    // Size of packet = Header (4 bytes) + 1 Entry (20 bytes)
    int payload_len = RIP_HEADER_SIZE + RIP_ENTRY_SIZE;

    // Send to port 520
    udp_send(udp_layer, 0, dest_ip, RIP_PORT, (unsigned char *)&msg, payload_len);
    printf("RIPv2 Request sent to %s\n", server_ip_str);

    uint16_t src_port;
    ipv4_addr_t src_ip;
    unsigned char buffer[1500]; // Buffer for response

    // Wait for response
    int len = udp_rcv(udp_layer, &src_port, src_ip, buffer, 1500, 5000);

    printf("[DEBUG] udp_rcv returned %d bytes.\n", len);

    if (len > 0) {
        ripv2_msg_t *response = (ripv2_msg_t *)buffer;

        if (response->command == RIP_COMMAND_RESPONSE) {
            // Check if there are any entries
            if (len < 24) {
                printf("Received RIPv2 Response, but it contains no routes.\n");
                udp_close(udp_layer); // Buena práctica cerrar antes de salir
                return 0;
            }
            int num_entries = (len - RIP_HEADER_SIZE) / RIP_ENTRY_SIZE; // Header is 4 bytes, Entry is 20

            printf("Received RIPv2 Response with %d entries:\n", num_entries);
            printf("------------------------------------------\n");

            for (int i = 0; i < num_entries; i++) {
                ripv2_entry_t *entry = &response->entries[i];

                char ip_str[16], mask_str[16], nexthop_str[16];

                // Convertimos los datos crudos a strings
                ipv4_addr_str(entry->ip, ip_str);
                ipv4_addr_str(entry->mask, mask_str);
                ipv4_addr_str(entry->next_hop, nexthop_str);
                uint32_t metric = ntohl(entry->metric);

                // Variable para guardar el Next Hop "real" para mostrar al usuario
                char display_nexthop[64];

                // Verificamos si el router nos mandó 0.0.0.0
                if (strcmp(nexthop_str, "0.0.0.0") == 0) {
                    // Si es 0.0.0.0, significa "Úsame a mí (el remitente) como gateway"
                    snprintf(display_nexthop, sizeof(display_nexthop), "%s (Sender)", server_ip_str);
                } else {
                    // Si no es 0.0.0.0, usamos la IP que venía en el paquete
                    snprintf(display_nexthop, sizeof(display_nexthop), "%s", nexthop_str);
                }

                printf("  Entry %d: Subnet %s/%s | Metric: %u | Next Hop: %s\n",
                       i + 1, ip_str, mask_str, metric, display_nexthop);
            }
            printf("------------------------------------------\n");

        } else {
            printf("Received a packet that was not a RIP Response (Command: %d).\n", response->command);
        }
    } else {
        printf("Failed to receive RIPv2 Response. The request may have timed out.\n");
    }

    // Close the UDP layer
    udp_close(udp_layer);
    return 0;
}
