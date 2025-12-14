#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ipv4.h"

#define PROTOCOL 123

void print_usage(const char *prog_name) {
    printf("Uso: %s <config_file> <route_table_file> <dest_ip> <message> [-e]\n", prog_name);
    printf("  -e: Corromper checksum IPv4 intencionadamente (opcional)\n");
}

int main(int argc, char *argv[]) {
    int corrupt = 0;

    // Se esperan al menos 5 argumentos (nombre_prog + config + rutas + ip + mensaje)
    if (argc < 5) {
        print_usage(argv[0]);
        return -1;
    }

    // Comprobar flag opcional -e al final
    if (argc >= 6 && strcmp(argv[5], "-e") == 0) {
        corrupt = 1;
    }

    char *config_file = argv[1];
    char *route_table_file = argv[2];
    char *server_ip_str = argv[3];
    char *message = argv[4];

    // 1. Abrir capa IPv4
    ipv4_layer_t *layer = ipv4_open(config_file, route_table_file);
    if (!layer) {
        fprintf(stderr, "Error al inicializar la capa IPv4.\n");
        return 1;
    }

    // 2. Parsear IP destino
    ipv4_addr_t dest_addr;
    if (ipv4_str_addr(server_ip_str, dest_addr) != 0) {
        fprintf(stderr, "Error: Dirección IP destino inválida '%s'.\n", server_ip_str);
        ipv4_close(layer);
        return 1;
    }

    printf("Enviando mensaje a %s (Corrupt=%d)...\n", server_ip_str, corrupt);

    // 3. Enviar paquete
    int bytes_sent = ipv4_send(layer, dest_addr, PROTOCOL, (unsigned char *)message, strlen(message), corrupt);

    if (bytes_sent > 0) {
        printf("Paquete enviado correctamente (%d bytes payload).\n", bytes_sent);
    } else {
        fprintf(stderr, "Error al enviar paquete. ipv4_send retornó: %d\n", bytes_sent);
    }

    // 4. Cerrar
    ipv4_close(layer);
    return 0;
}