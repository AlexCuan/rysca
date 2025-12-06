#include "ripv2_route_table.h"
#include "../ipv4/ipv4_route_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h> // Para inet_ntoa si fuera necesario, o usar ipv4_addr_str

/* Estructura opaca para la tabla de rutas RIPv2 */
struct ripv2_route_table {
  ripv2_route_t * routes[IPv4_ROUTE_TABLE_SIZE];
};

/* ripv2_route_t * ripv2_route_create
 * ( ipv4_addr_t subnet, ipv4_addr_t mask, ipv4_addr_t next_hop, uint32_t metric )
 * * DESCRIPCIÓN: 
 * Crea una ruta RIPv2 e inicializa sus temporizadores.
 */
ripv2_route_t * ripv2_route_create
( ipv4_addr_t subnet, ipv4_addr_t mask, ipv4_addr_t next_hop, uint32_t metric )
{
  ripv2_route_t * route = (ripv2_route_t *) malloc(sizeof(ripv2_route_t));

  if (route != NULL) {
    memcpy(route->subnet, subnet, IPv4_ADDR_SIZE);
    memcpy(route->mask, mask, IPv4_ADDR_SIZE);
    memcpy(route->next_hop, next_hop, IPv4_ADDR_SIZE);
    route->metric = metric;
    route->route_tag = 0;
    route->last_updated = time(NULL); // Inicializar timestamp actual
    route->is_garbage = 0;
  }
  
  return route;
}

/* void ripv2_route_free ( ripv2_route_t * route ) */
void ripv2_route_free ( ripv2_route_t * route )
{
  if (route != NULL) {
    free(route);
  }
}

/* void ripv2_route_print ( ripv2_route_t * route )
 * Imprime el estado de la ruta RIP, incluyendo temporizadores.
 */
void ripv2_route_print ( ripv2_route_t * route)
{
  if (route != NULL) {
    char subnet_str[IPv4_STR_MAX_LENGTH];
    char mask_str[IPv4_STR_MAX_LENGTH];
    char nh_str[IPv4_STR_MAX_LENGTH];
    
    ipv4_addr_str(route->subnet, subnet_str);
    ipv4_addr_str(route->mask, mask_str);
    ipv4_addr_str(route->next_hop, nh_str);
    
    // Calcular segundos desde la última actualización
    double seconds_since_update = difftime(time(NULL), route->last_updated);

    printf("%s/%s -> Nexthop: %s | Metric: %2d | Age: %3.0fs | Garbage: %s\n", 
           subnet_str, mask_str, nh_str, route->metric, 
           seconds_since_update, 
           (route->is_garbage ? "YES" : "NO"));
  }
}

/* ripv2_route_table_t * ripv2_route_table_create() */
ripv2_route_table_t * ripv2_route_table_create()
{
  ripv2_route_table_t * table;

  table = (ripv2_route_table_t *) malloc(sizeof(struct ripv2_route_table));
  if (table != NULL) {
    int i;
    for (i=0; i<IPv4_ROUTE_TABLE_SIZE; i++) {
      table->routes[i] = NULL;
    }
  }

  return table;
}

/* void ripv2_route_table_free ( ripv2_route_table_t * table ) */
void ripv2_route_table_free ( ripv2_route_table_t * table )
{
  if (table != NULL) {
    int i;
    for (i=0; i<IPv4_ROUTE_TABLE_SIZE; i++) {
      if (table->routes[i] != NULL) {
        ripv2_route_free(table->routes[i]);
        table->routes[i] = NULL;
      }
    }
    free(table);
  }
}

/* int ripv2_route_table_add ( ripv2_route_table_t * table, ripv2_route_t * route ) */
int ripv2_route_table_add ( ripv2_route_table_t * table, ripv2_route_t * route )
{
  int route_index = -1;

  if (table != NULL && route != NULL) {
    int i;
    for (i=0; i<IPv4_ROUTE_TABLE_SIZE; i++) {
      if (table->routes[i] == NULL) {
        table->routes[i] = route;
        route_index = i;
        break;
      }
    }
  }

  return route_index;
}

/* ripv2_route_t * ripv2_route_table_remove ( ripv2_route_table_t * table, int index ) */
ripv2_route_t * ripv2_route_table_remove ( ripv2_route_table_t * table, int index )
{
  ripv2_route_t * removed_route = NULL;
  
  if ((table != NULL) && (index >= 0) && (index < IPv4_ROUTE_TABLE_SIZE)) {
    removed_route = table->routes[index];
    table->routes[index] = NULL;
  }

  return removed_route;
}

/* ripv2_route_t * ripv2_route_table_get ( ripv2_route_table_t * table, int index ) */
ripv2_route_t * ripv2_route_table_get ( ripv2_route_table_t * table, int index )
{
  if ((table != NULL) && (index >= 0) && (index < IPv4_ROUTE_TABLE_SIZE)) {
    return table->routes[index];
  }
  return NULL;
}

int ripv2_route_table_size(ripv2_route_table_t *table)
{
    if (table == NULL) {
        return 0;
    }
    return IPv4_ROUTE_TABLE_SIZE;
}

/* ripv2_route_t * ripv2_route_table_lookup ( ... )
 * IMPORTANTE: Para RIP, esta función busca coincidencia EXACTA de subnet y mask.
 * Se usa para saber si una ruta recibida ya existe y actualizarla.
 */
ripv2_route_t * ripv2_route_table_lookup ( ripv2_route_table_t * table, 
                                           ipv4_addr_t subnet, ipv4_addr_t mask )
{
  if (table != NULL) {
    int i;
    for (i=0; i<IPv4_ROUTE_TABLE_SIZE; i++) {
      ripv2_route_t * route = table->routes[i];
      if (route != NULL) {
        // Comparación de memoria exacta para Subred y Máscara
        if (memcmp(route->subnet, subnet, IPv4_ADDR_SIZE) == 0 &&
            memcmp(route->mask, mask, IPv4_ADDR_SIZE) == 0) {
          return route; // Encontrado
        }
      }
    }
  }
  return NULL; // No encontrado
}

/* void ripv2_route_table_print ( ripv2_route_table_t * table ) */
void ripv2_route_table_print ( ripv2_route_table_t * table )
{
  if (table != NULL) {
    printf("\n--- RIPv2 Routing Table ---\n");
    int i;
    for (i=0; i<IPv4_ROUTE_TABLE_SIZE; i++) {
      if (table->routes[i] != NULL) {
        ripv2_route_print(table->routes[i]);
      }
    }
    printf("---------------------------\n");
  }
}

/* Helpers para lectura de fichero (simplificado) */
int ripv2_route_table_read ( char * filename, ripv2_route_table_t * table )
{
  FILE * file = fopen(filename, "r");
  if (file == NULL) {
    return -1;
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#' || line[0] == '\n') continue;

    char subnet_str[32], mask_str[32], nh_str[32];
    int metric;
    
    // Formato esperado: Subnet Mask NextHop Metric
    if (sscanf(line, "%s %s %s %d", subnet_str, mask_str, nh_str, &metric) == 4) {
      ipv4_addr_t subnet, mask, nh;
      if (ipv4_str_addr(subnet_str, subnet) == 0 &&
          ipv4_str_addr(mask_str, mask) == 0 &&
          ipv4_str_addr(nh_str, nh) == 0) {
          
          ripv2_route_t * new_route = ripv2_route_create(subnet, mask, nh, metric);
          ripv2_route_table_add(table, new_route);
      }
    }
  }
  fclose(file);
  return 0;
}