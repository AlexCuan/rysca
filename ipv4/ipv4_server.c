#include <stdio.h>
#include <stdlib.h>
#include "ipv4.h"

#define PROTOCOL 123

void print_usage(const char* prog_name)
{
    printf("Uso: %s <config_file> <route_table_file>\n", prog_name);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        print_usage(argv[0]);
        return -1;
    }

    char* config_file = argv[1];
    char* route_table_file = argv[2];

    // 1. Initialise the IPv4 layer
    ipv4_layer_t* layer = ipv4_open(config_file, route_table_file);
    if (!layer)
    {
        fprintf(stderr, "Error al inicializar la capa IPv4.\n");
        return 1;
    }

    unsigned char buffer[1500];
    ipv4_addr_t sender;
    char sender_str[IPv4_STR_MAX_LENGTH];

    printf("Servidor IPv4 escuchando (Protocolo %d)...\n", PROTOCOL);

    while (1)
    {
        int len = ipv4_recv(layer, PROTOCOL, buffer, sender, NULL, sizeof(buffer), -1);

        if (len == 0)
        {
            continue; // Timeout: keep listening
        }

        if (len > 0)
        {
            ipv4_addr_str(sender, sender_str);
            printf("\n--- Paquete IPv4 Recibido ---\n");
            printf("Origen: %s\n", sender_str);
            printf("Longitud: %d bytes\n", len);
            printf("Contenido: \"%.*s\"\n", len, buffer);
        }
        else
        {
            fprintf(stderr, "Error en ipv4_recv o conexión cerrada.\n");
            break;
        }
    }

    ipv4_close(layer);
    return 0;
}
