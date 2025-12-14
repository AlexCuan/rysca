#include "eth.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

int main ( int argc, char * argv[] )
{
  char * myself = basename(argv[0]);
  if (argc != 3) {
    printf("Uso: %s <iface> <tipo>\n", myself);
    printf("       <iface>: Nombre de la interfaz Ethernet\n");
    printf("        <tipo>: Campo 'Tipo' de las tramas Ethernet (ej. 0x0800)\n");
    exit(-1);
  }

  char * iface_name = argv[1];
  char* eth_type_str = argv[2];
  char* endptr;
  int eth_type_int = (int) strtol(eth_type_str, &endptr, 0);

  if ((*endptr != '\0') || (eth_type_int < 0) || (eth_type_int > 0x0000FFFF)) {
    fprintf(stderr, "%s: Tipo Ethernet incorrecto: '%s'\n", myself, eth_type_str);
    exit(-1);
  }
  uint16_t eth_type = (uint16_t) eth_type_int;

  eth_iface_t * eth_iface = eth_open(iface_name);
  if (eth_iface == NULL) {
    fprintf(stderr, "%s: ERROR en eth_open(\"%s\")\n", myself, iface_name);
    exit(-1);
  }

  mac_addr_t server_addr;
  eth_getaddr(eth_iface, server_addr);
  char server_addr_str[MAC_STR_LENGTH];
  mac_addr_str(server_addr, server_addr_str);

  printf("Servidor Ethernet escuchando en %s (%s). Tipo: 0x%04x\n",
         iface_name, server_addr_str, eth_type);

  // --- BUCLE INFINITO ---
  while(1) {
    unsigned char buffer[ETH_MTU];
    mac_addr_t src_addr;

    // Timeout -1 para esperar indefinidamente
    int payload_len = eth_recv(eth_iface, src_addr, eth_type, buffer, ETH_MTU, -1);

    if (payload_len == -1) {
      // ERROR NO FATAL: Imprimimos y seguimos escuchando
      fprintf(stderr, "%s: Error al recibir trama (eth_recv returned -1). Ignorando...\n", myself);
      continue;
    }

    char src_addr_str[MAC_STR_LENGTH];
    mac_addr_str(src_addr, src_addr_str);

    printf("\n[RECV] %d bytes de %s\n", payload_len, src_addr_str);
    // print_pkt(buffer, payload_len, 0); // Descomentar para ver contenido en hex

    /* ECHO: Enviar la misma trama de vuelta */
    printf("[SEND] Enviando Echo a %s...\n", src_addr_str);

    int len = eth_send(eth_iface, src_addr, eth_type, buffer, payload_len);
    if (len == -1) {
      fprintf(stderr, "%s: ERROR en eth_send()\n", myself);
    }
  }

  eth_close(eth_iface);
  return 0;
}