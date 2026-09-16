#include "eth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#define DEFAULT_PAYLOAD_LENGTH 64

int main(int argc, char* argv[])
{
    char* myself = basename(argv[0]);

    if (argc < 4 || argc > 5)
    {
        printf("Uso: %s <iface> <tipo> <mac_destino> [<mensaje> | <longitud>]\n", myself);
        exit(-1);
    }

    char* iface_name = argv[1];
    char* eth_type_str = argv[2];
    uint16_t eth_type = (uint16_t)strtol(eth_type_str, NULL, 0);
    char* server_addr_str = argv[3];

    mac_addr_t server_addr;
    if (mac_str_addr(server_addr_str, server_addr) != 0)
    {
        fprintf(stderr, "MAC destino inválida: %s\n", server_addr_str);
        exit(-1);
    }

    // --- PAYLOAD PREPARATION ---
    unsigned char payload[ETH_MTU];
    int payload_len = DEFAULT_PAYLOAD_LENGTH;
    int is_text_msg = 0;

    if (argc == 5)
    {
        // Work out whether it is a number (length) or text
        char* endptr;
        long val = strtol(argv[4], &endptr, 10);

        if (*endptr == '\0')
        {
            // Plain number: use it as the length
            payload_len = (int)val;
            if (payload_len > ETH_MTU) payload_len = ETH_MTU;
        }
        else
        {
            // Text: use it as the message
            strncpy((char*)payload, argv[4], ETH_MTU);
            payload_len = strlen(argv[4]);
            is_text_msg = 1;
        }
    }

    // If it is not text, fill with a numeric pattern
    if (!is_text_msg)
    {
        for (int i = 0; i < payload_len; i++) payload[i] = (unsigned char)(i % 256);
    }

    // --- OPEN THE INTERFACE ---
    eth_iface_t* eth_iface = eth_open(iface_name);
    if (!eth_iface)
    {
        fprintf(stderr, "Error abriendo interfaz %s\n", iface_name);
        exit(-1);
    }

    // --- SEND ---
    printf("Enviando %d bytes a %s (Tipo 0x%04x)...\n", payload_len, server_addr_str, eth_type);
    if (is_text_msg) printf("Mensaje: \"%s\"\n", payload);

    if (eth_send(eth_iface, server_addr, eth_type, payload, payload_len) == -1)
    {
        fprintf(stderr, "Error en eth_send\n");
        exit(-1);
    }

    // --- RECEIVE (wait for the reply) ---
    printf("Esperando respuesta (Timeout 2s)...\n");

    unsigned char buffer[ETH_MTU];
    mac_addr_t src_addr;
    int len = eth_recv(eth_iface, src_addr, eth_type, buffer, ETH_MTU, 2000);

    if (len > 0)
    {
        char src_str[MAC_STR_LENGTH];
        mac_addr_str(src_addr, src_str);
        printf("Respuesta recibida de %s (%d bytes):\n", src_str, len);

        if (is_text_msg)
        {
            printf("Contenido: \"%.*s\"\n", len, buffer);
        }
        else
        {
            print_pkt(buffer, len, 0);
        }
    }
    else if (len == 0)
    {
        printf("Timeout: No hubo respuesta del servidor.\n");
    }
    else
    {
        fprintf(stderr, "Error en eth_recv\n");
    }

    eth_close(eth_iface);
    return 0;
}
